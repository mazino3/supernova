#include "RenderSystem.h"
#include "object/Camera.h"
#include "math/Quaternion.h"
#include "Scene.h"
#include "Engine.h"
#include "System.h"
#include "render/Render.h"
#include "render/ObjectRender.h"
#include "render/SystemRender.h"
#include "pool/ShaderPool.h"
#include "pool/TexturePool.h"
#include "math/Vector3.h"
#include "math/Angle.h"
#include <memory>

using namespace Supernova;

TextureRender RenderSystem::emptyWhite;
TextureRender RenderSystem::emptyBlack;
TextureRender RenderSystem::emptyNormal;

bool RenderSystem::emptyTexturesCreated = false;

RenderSystem::RenderSystem(Scene* scene): SubSystem(scene){
	signature.set(scene->getComponentType<Transform>());
	signature.set(scene->getComponentType<MeshComponent>());

	hasLights = true;
}

void RenderSystem::load(){
	createEmptyTextures();
	depthRender.setClearColor(Vector4(1.0, 1.0, 1.0, 1.0));

	//TODO: Remove from here, to can create lights at runtime
	auto lights = scene->getComponentArray<LightComponent>();	
	for (int i = 0; i < lights->size(); i++){
		LightComponent& light = lights->getComponentFromIndex(i);
		light.lightFb.createFramebuffer(2048, 2048);
	}
}

void RenderSystem::createEmptyTextures(){
	if (!emptyTexturesCreated){
		TextureDataSize texData[1];

    	uint32_t pixels[64];

		texData[0].data = (void*)pixels;
		texData[0].size = 8 * 8 * 4;

		for (int i = 0; i < 64; i++) {
        	pixels[i] = 0xFFFFFFFF;
    	}

		emptyWhite.createTexture("empty|white", 8, 8, ColorFormat::RGBA, TextureType::TEXTURE_2D, 1, texData);

		for (int i = 0; i < 64; i++) {
        	pixels[i] = 0xFF000000;
    	}

		emptyBlack.createTexture("empty|black", 8, 8, ColorFormat::RGBA, TextureType::TEXTURE_2D, 1, texData);

		for (int i = 0; i < 64; i++) {
        	pixels[i] = 0xFF808080;
    	}

		emptyNormal.createTexture("empty|normal", 8, 8, ColorFormat::RGBA, TextureType::TEXTURE_2D, 1, texData);

		emptyTexturesCreated = true;
	}
}

u_lighting_t RenderSystem::collectLights(){
	u_lighting_t lightsBlock;

	auto lights = scene->getComponentArray<LightComponent>();

	int numLights = lights->size();
	if (numLights > MAX_LIGHTS)
		numLights = MAX_LIGHTS;
	
	for (int i = 0; i < numLights; i++){
		LightComponent& light = lights->getComponentFromIndex(i);
		Entity entity = lights->getEntity(i);
		Transform* transform = scene->findComponent<Transform>(entity);

		Vector3 worldPosition;
		if (transform){
			worldPosition = Vector3(transform->worldPosition);
		}

		int type = 0;
		if (light.type == LightType::POINT)
			type = 1;
		if (light.type == LightType::SPOT)
			type = 2;

		lightsBlock.direction_range[i] = Vector4(light.worldDirection.x, light.worldDirection.y, light.worldDirection.z, light.range);
		lightsBlock.color_intensity[i] = Vector4(light.color.x, light.color.y, light.color.z, light.intensity);
		lightsBlock.position_type[i] = Vector4(worldPosition.x, worldPosition.y, worldPosition.z, (float)type);
		lightsBlock.inner_outer_ConeCos[i] = Vector4(light.innerConeCos, light.outerConeCos, 0.0f, 0.0);

		if (light.type == LightType::DIRECTIONAL){
			Matrix4 projectionMatrix = Matrix4::orthoMatrix(-500, 500, -500, 500, -500, 500);
			Matrix4 viewMatrix = Matrix4::lookAtMatrix(transform->worldPosition, light.direction, Vector3(0, 1, 0));

			light.lightViewProjection = projectionMatrix * viewMatrix;
		}
	}

	// Setting intensity of other lights to zero
	for (int i = numLights; i < MAX_LIGHTS; i++){
		lightsBlock.color_intensity[i].w = 0.0;
	}

	return lightsBlock;
}

bool RenderSystem::loadMesh(MeshComponent& mesh, FramebufferRender& lightFb){

	bufferNameToRender.clear();

	std::map<std::string, unsigned int> bufferStride;

	for (auto const& buf : mesh.buffers){
		buf.second->getRender()->createBuffer(buf.second->getSize(), buf.second->getData(), buf.second->getBufferType(), false);
		bufferNameToRender[buf.first] = buf.second->getRender();
		bufferStride[buf.first] = buf.second->getStride();
	}

	for (int i = 0; i < mesh.numSubmeshes; i++){

		mesh.submeshes[i].hasNormalMap = false;
		mesh.submeshes[i].hasTangent = false;
		mesh.submeshes[i].hasVertexColor = false;

		ObjectRender* render = &mesh.submeshes[i].render;

		render->beginLoad(mesh.submeshes[i].primitiveType, false);

		for (auto const& buf : mesh.buffers){
        	if (buf.second->isRenderAttributes()) {
            	for (auto const &attr : buf.second->getAttributes()) {
					if (attr.first == AttributeType::TANGENT){
						mesh.submeshes[i].hasTangent = true;
					}
					if (attr.first == AttributeType::COLOR){
						mesh.submeshes[i].hasVertexColor = true;
					}
            	}
        	}
    	}
		for (auto const& attr : mesh.submeshes[i].attributes){
			if (attr.first == AttributeType::TANGENT){
				mesh.submeshes[i].hasTangent = true;
			}
			if (attr.first == AttributeType::COLOR){
				mesh.submeshes[i].hasVertexColor = true;
			}
		}

		if (!mesh.submeshes[i].material.normalTexture.empty()){
			mesh.submeshes[i].hasNormalMap = true;
		}


		ShaderType shaderType = ShaderType::MESH;

		bool p_unlit = false;
		bool p_punctual = false;
		bool p_hasNormalMap = false;
		bool p_hasNormal = false;
		bool p_hasTangent = false;
		bool p_castShadows = false;

		if (hasLights){
			p_punctual = true;

			p_hasNormal = true;
			if (mesh.submeshes[i].hasTangent){
				p_hasTangent = true;
			}
			if (mesh.submeshes[i].hasNormalMap){
				p_hasNormalMap = true;
			}
			if (mesh.castShadows){
				p_castShadows = true;
			}
		}else{
			p_unlit = true;
		}

		mesh.submeshes[i].shaderProperties = ShaderPool::getMeshProperties(p_unlit, true, false, p_punctual, p_castShadows, p_hasNormal, p_hasNormalMap, p_hasTangent, false, true);
		mesh.submeshes[i].shader = ShaderPool::get(shaderType, mesh.submeshes[i].shaderProperties);
		if (hasLights && mesh.castShadows){
			mesh.submeshes[i].depthShader = ShaderPool::get(ShaderType::DEPTH, "");
			if (!mesh.submeshes[i].depthShader->isCreated())
				return false;
		}
		if (!mesh.submeshes[i].shader->isCreated())
			return false;
		render->loadShader(mesh.submeshes[i].shader.get());
		ShaderData& shaderData = mesh.submeshes[i].shader.get()->shaderData;

		mesh.submeshes[i].slotVSParams = shaderData.getUniformIndex(UniformType::PBR_VS_PARAMS, ShaderStageType::VERTEX);
		mesh.submeshes[i].slotFSParams = shaderData.getUniformIndex(UniformType::PBR_FS_PARAMS, ShaderStageType::FRAGMENT);
		if (hasLights){
			mesh.submeshes[i].slotFSLighting = shaderData.getUniformIndex(UniformType::FS_LIGHTING, ShaderStageType::FRAGMENT);
			if (mesh.castShadows){
				mesh.submeshes[i].slotVSLighting = shaderData.getUniformIndex(UniformType::VS_LIGHTING, ShaderStageType::VERTEX);
			}
		}

		TextureRender* textureRender = NULL;
		textureRender = mesh.submeshes[i].material.baseColorTexture.getRender();
		if (textureRender)
			render->loadTexture(shaderData.getTextureIndex(TextureShaderType::BASECOLOR, ShaderStageType::FRAGMENT), ShaderStageType::FRAGMENT, textureRender);
		else
			render->loadTexture(shaderData.getTextureIndex(TextureShaderType::BASECOLOR, ShaderStageType::FRAGMENT), ShaderStageType::FRAGMENT, &emptyWhite);

		if (hasLights){
			TextureRender* textureRender = NULL;
			int slotTex = -1;
			
			textureRender = mesh.submeshes[i].material.metallicRoughnessTexture.getRender();
			slotTex = shaderData.getTextureIndex(TextureShaderType::METALLICROUGHNESS, ShaderStageType::FRAGMENT);
			if (textureRender)
				render->loadTexture(slotTex, ShaderStageType::FRAGMENT, textureRender);
			else
				render->loadTexture(slotTex, ShaderStageType::FRAGMENT, &emptyWhite);

			textureRender = mesh.submeshes[i].material.normalTexture.getRender();
			slotTex = shaderData.getTextureIndex(TextureShaderType::NORMAL, ShaderStageType::FRAGMENT);
			if (textureRender)
				render->loadTexture(slotTex, ShaderStageType::FRAGMENT, textureRender);
			else
				render->loadTexture(slotTex, ShaderStageType::FRAGMENT, &emptyNormal);

			textureRender = mesh.submeshes[i].material.occlusionTexture.getRender();
			slotTex = shaderData.getTextureIndex(TextureShaderType::OCCULSION, ShaderStageType::FRAGMENT);
			if (textureRender)	
				render->loadTexture(slotTex, ShaderStageType::FRAGMENT, textureRender);
			else
				render->loadTexture(slotTex, ShaderStageType::FRAGMENT, &emptyWhite);

			textureRender = mesh.submeshes[i].material.emissiveTexture.getRender();
			slotTex = shaderData.getTextureIndex(TextureShaderType::EMISSIVE, ShaderStageType::FRAGMENT);
			if (textureRender)	
				render->loadTexture(slotTex, ShaderStageType::FRAGMENT, textureRender);
			else
				render->loadTexture(slotTex, ShaderStageType::FRAGMENT, &emptyBlack);

			if (hasLights && mesh.castShadows){
				slotTex = shaderData.getTextureIndex(TextureShaderType::SHADOWMAP1, ShaderStageType::FRAGMENT);
				render->loadTexture(slotTex, ShaderStageType::FRAGMENT, lightFb.getColorTexture());
			}
		}

		if (Engine::isAutomaticTransparency() && !mesh.transparency){
			if (mesh.submeshes[i].material.baseColorTexture.isTransparent() || mesh.submeshes[i].material.baseColorFactor.w != 1.0){
				mesh.transparency = true;
			}
		}
	
		unsigned int vertexBufferCount = 0;
		unsigned int indexCount = 0;

		for (auto const& buf : mesh.buffers){
        	if (buf.first == mesh.defaultBuffer) {
				vertexBufferCount = buf.second->getCount();
        	}
        	if (buf.second->isRenderAttributes()) {
            	for (auto const &attr : buf.second->getAttributes()) {
					render->loadAttribute(shaderData.getAttrIndex(attr.first), buf.second->getRender(), attr.second.getElements(), attr.second.getDataType(), buf.second->getStride(), attr.second.getOffset(), attr.second.getNormalized());
            	}
        	}
			if (buf.second->getBufferType() == BufferType::INDEX_BUFFER){
				indexCount = buf.second->getCount();
				Attribute indexattr = buf.second->getAttributes()[AttributeType::INDEX];
				render->loadIndex(buf.second->getRender(), indexattr.getDataType(), indexattr.getOffset());
			}
    	}

		for (auto const& attr : mesh.submeshes[i].attributes){
			if (attr.first == AttributeType::INDEX){
				indexCount = attr.second.getCount();
				render->loadIndex(bufferNameToRender[attr.second.getBuffer()], attr.second.getDataType(), attr.second.getOffset());
			}else{
				render->loadAttribute(shaderData.getAttrIndex(attr.first), bufferNameToRender[attr.second.getBuffer()], attr.second.getElements(), attr.second.getDataType(), bufferStride[attr.second.getBuffer()], attr.second.getOffset(), attr.second.getNormalized());
			}
		}

		if (indexCount > 0){
			mesh.submeshes[i].vertexCount = indexCount;
		}else{
			mesh.submeshes[i].vertexCount = vertexBufferCount;
		}

		render->endLoad();

		//----------Start depth shader---------------
		if (hasLights && mesh.castShadows){
			ObjectRender* depthRender = &mesh.submeshes[i].depthRender;

			depthRender->beginLoad(mesh.submeshes[i].primitiveType, true);

			depthRender->loadShader(mesh.submeshes[i].depthShader.get());
			ShaderData& depthShaderData = mesh.submeshes[i].depthShader.get()->shaderData;

			mesh.submeshes[i].slotVSDepthParams = depthShaderData.getUniformIndex(UniformType::DEPTH_VS_PARAMS, ShaderStageType::VERTEX);

			for (auto const& buf : mesh.buffers){
        		if (buf.second->isRenderAttributes()) {
					if (buf.second->getAttributes().count(AttributeType::POSITION)){
						Attribute posattr = buf.second->getAttributes()[AttributeType::POSITION];
						depthRender->loadAttribute(depthShaderData.getAttrIndex(AttributeType::POSITION), buf.second->getRender(), posattr.getElements(), posattr.getDataType(), buf.second->getStride(), posattr.getOffset(), posattr.getNormalized());
					}
        		}
				if (buf.second->getBufferType() == BufferType::INDEX_BUFFER){
					indexCount = buf.second->getCount();
					Attribute indexattr = buf.second->getAttributes()[AttributeType::INDEX];
					depthRender->loadIndex(buf.second->getRender(), indexattr.getDataType(), indexattr.getOffset());
				}
    		}

			for (auto const& attr : mesh.submeshes[i].attributes){
				if (attr.first == AttributeType::INDEX){
					depthRender->loadIndex(bufferNameToRender[attr.second.getBuffer()], attr.second.getDataType(), attr.second.getOffset());
				}else if (attr.first == AttributeType::POSITION){
					depthRender->loadAttribute(depthShaderData.getAttrIndex(AttributeType::POSITION), bufferNameToRender[attr.second.getBuffer()], attr.second.getElements(), attr.second.getDataType(), bufferStride[attr.second.getBuffer()], attr.second.getOffset(), attr.second.getNormalized());
				}
			}

			depthRender->endLoad();
		}
		//----------End depth shader---------------
	}

	mesh.loaded = true;

	return true;
}

void RenderSystem::drawMesh(MeshComponent& mesh, Transform& transform, Transform& camTransform, u_lighting_t& lights, Matrix4 lightSpaceMatrix){
	if (mesh.loaded){
		for (int i = 0; i < mesh.numSubmeshes; i++){
			ObjectRender* render = &mesh.submeshes[i].render;
			
			u_fs_pbrParams_t pbrParams_fs;
			pbrParams_fs.baseColorFactor = mesh.submeshes[i].material.baseColorFactor;
			pbrParams_fs.metallicFactor = mesh.submeshes[i].material.metallicFactor;
			pbrParams_fs.roughnessFactor = mesh.submeshes[i].material.roughnessFactor;
			pbrParams_fs.emissiveFactor = mesh.submeshes[i].material.emissiveFactor;
			pbrParams_fs.eyePos = camTransform.worldPosition;

			Matrix4 lightSpaces[MAX_LIGHTS];
			for (int i = 0; i < MAX_LIGHTS; i++){
				lightSpaces[i] = lightSpaceMatrix;
			}

			render->beginDraw();

			if (hasLights){
				render->applyUniform(mesh.submeshes[i].slotFSLighting, ShaderStageType::FRAGMENT, UniformDataType::FLOAT, 16 * MAX_LIGHTS, &lights);
				if (mesh.castShadows){
					render->applyUniform(mesh.submeshes[i].slotVSLighting, ShaderStageType::VERTEX, UniformDataType::FLOAT, 16 * MAX_LIGHTS, &lightSpaces);
				}
			}

			render->applyUniform(mesh.submeshes[i].slotFSParams, ShaderStageType::FRAGMENT, UniformDataType::FLOAT, 16, &pbrParams_fs);

			//model, normal and mvp matrix
			render->applyUniform(mesh.submeshes[i].slotVSParams, ShaderStageType::VERTEX, UniformDataType::FLOAT, 48, &transform.modelMatrix);

			render->draw(mesh.submeshes[i].vertexCount);
		}
	}
}

void RenderSystem::drawMeshDepth(MeshComponent& mesh, Matrix4 modelLightSpaceMatrix){
	if (mesh.loaded && hasLights && mesh.castShadows){
		for (int i = 0; i < mesh.numSubmeshes; i++){
			ObjectRender* depthRender = &mesh.submeshes[i].depthRender;

			depthRender->beginDraw();
			
			//mvp matrix
			depthRender->applyUniform(mesh.submeshes[i].slotVSDepthParams, ShaderStageType::VERTEX, UniformDataType::FLOAT, 16, &modelLightSpaceMatrix);

			depthRender->draw(mesh.submeshes[i].vertexCount);
		}
	}
}

bool RenderSystem::loadSky(SkyComponent& sky){

	ObjectRender* render = &sky.render;

	render->beginLoad(PrimitiveType::TRIANGLES, false);

	ShaderType shaderType = ShaderType::SKYBOX;

	sky.shader = ShaderPool::get(shaderType, "");
	if (!sky.shader->isCreated())
		return false;
	render->loadShader(sky.shader.get());
	ShaderData& shaderData = sky.shader.get()->shaderData;

	sky.slotVSParams = shaderData.getUniformIndex(UniformType::VIEWPROJECTIONSKY, ShaderStageType::VERTEX);

	if (sky.texture.load()){
		render->loadTexture(shaderData.getTextureIndex(TextureShaderType::SKYCUBE, ShaderStageType::FRAGMENT), ShaderStageType::FRAGMENT, sky.texture.getRender());
	} else {
		return false;
	}

	sky.buffer->getRender()->createBuffer(sky.buffer->getSize(), sky.buffer->getData(), sky.buffer->getBufferType(), false);

	if (sky.buffer->isRenderAttributes()) {
        for (auto const &attr : sky.buffer->getAttributes()) {
			render->loadAttribute(shaderData.getAttrIndex(attr.first), sky.buffer->getRender(), attr.second.getElements(), attr.second.getDataType(), sky.buffer->getStride(), attr.second.getOffset(), attr.second.getNormalized());
        }
    }

	sky.loaded = true;

	render->endLoad();

	return true;
}

void RenderSystem::drawSky(SkyComponent& sky){
	if (sky.loaded){
		ObjectRender* render = &sky.render;
		if (render){
			render->beginDraw();
			render->applyUniform(sky.slotVSParams, ShaderStageType::VERTEX, UniformDataType::FLOAT, 16, &sky.skyViewProjectionMatrix);
			render->draw(36);
		}
	}
}

void RenderSystem::updateTransform(Transform& transform){
	Matrix4 scaleMatrix = Matrix4::scaleMatrix(transform.scale);
    Matrix4 translateMatrix = Matrix4::translateMatrix(transform.position);
    Matrix4 rotationMatrix = transform.rotation.getRotationMatrix();

	transform.modelMatrix = translateMatrix * rotationMatrix * scaleMatrix;

	if (transform.parent != NULL_ENTITY){
		auto transformParent = scene->getComponent<Transform>(transform.parent);

		transform.modelMatrix = transformParent.modelMatrix * transform.modelMatrix;

    	transform.worldRotation = transformParent.worldRotation * transform.rotation;
		transform.worldScale = transformParent.worldScale * transform.scale;
    	transform.worldPosition = transform.modelMatrix * Vector3(0,0,0);
	}else{
		transform.worldRotation = transform.rotation;
    	transform.worldScale = transform.scale;
    	transform.worldPosition = transform.position;
	}

	if (hasLights){
		transform.normalMatrix = transform.modelMatrix.inverse().transpose();
	}
}

void RenderSystem::updateCamera(CameraComponent& camera, Transform& transform){
	//Update ProjectionMatrix
	if (camera.type == CameraType::CAMERA_2D){
		camera.projectionMatrix = Matrix4::orthoMatrix(camera.left, camera.right, camera.top, camera.bottom, camera.orthoNear, camera.orthoFar);
	}else if (camera.type == CameraType::CAMERA_ORTHO) {
		camera.projectionMatrix = Matrix4::orthoMatrix(camera.left, camera.right, camera.bottom, camera.top, camera.orthoNear, camera.orthoFar);
	}else if (camera.type == CameraType::CAMERA_PERSPECTIVE){
		camera.projectionMatrix = Matrix4::perspectiveMatrix(camera.y_fov, camera.aspect, camera.perspectiveNear, camera.perspectiveFar);
	}

	if (transform.parent != NULL_ENTITY){
        camera.worldView = transform.modelMatrix * (camera.view - transform.position);
        camera.worldUp = ((transform.modelMatrix * camera.up) - (transform.modelMatrix * Vector3(0,0,0))).normalize();
    }else{
        camera.worldView = camera.view;
        camera.worldUp = camera.up;
    }

	//Update ViewMatrix
	if (camera.type == CameraType::CAMERA_2D){
        camera.viewMatrix.identity();
    }else{
        camera.viewMatrix = Matrix4::lookAtMatrix(transform.worldPosition, camera.worldView, camera.worldUp);
    }

	camera.worldRight = Vector3(camera.viewMatrix[0][0], camera.viewMatrix[1][0], camera.viewMatrix[2][0]);

	//Update ViewProjectionMatrix
	camera.viewProjectionMatrix = camera.projectionMatrix * camera.viewMatrix;
}

void RenderSystem::updateSkyViewProjection(CameraComponent& camera){
	SkyComponent* sky = scene->findComponentFromIndex<SkyComponent>(0);
	if (sky){
		Matrix4 skyViewMatrix = camera.viewMatrix;

		skyViewMatrix.set(3,0,0);
    	skyViewMatrix.set(3,1,0);
    	skyViewMatrix.set(3,2,0);
    	skyViewMatrix.set(3,3,1);
    	skyViewMatrix.set(2,3,0);
    	skyViewMatrix.set(1,3,0);
    	skyViewMatrix.set(0,3,0);

		sky->skyViewProjectionMatrix = camera.projectionMatrix * skyViewMatrix;
	}
}

void RenderSystem::updateLightFromTransform(LightComponent& light, Transform& transform){
	light.worldDirection = transform.worldRotation * light.direction;
}

void RenderSystem::update(double dt){
	CameraComponent& camera =  scene->getComponent<CameraComponent>(scene->getCamera());
	Transform& cameraTransform =  scene->getComponent<Transform>(scene->getCamera());

	if (cameraTransform.needUpdate){
		camera.needUpdate = true;
	}

	auto transforms = scene->getComponentArray<Transform>();
	for (int i = 0; i < transforms->size(); i++){
		Transform& transform = transforms->getComponentFromIndex(i);
		
		if (transform.needUpdate){
			updateTransform(transform);

			Entity entity = transforms->getEntity(i);

			LightComponent* light = scene->findComponent<LightComponent>(entity);
			if (light){
				updateLightFromTransform(*light, transform);
			}
		}
	}

	if (camera.needUpdate){
		updateCamera(camera, cameraTransform);
		updateSkyViewProjection(camera);
	}

	for (int i = 0; i < transforms->size(); i++){
		Transform& transform = transforms->getComponentFromIndex(i);

		if (camera.needUpdate || transform.needUpdate){
			transform.modelViewProjectionMatrix = camera.viewProjectionMatrix * transform.modelMatrix;
			transform.distanceToCamera = (cameraTransform.worldPosition - transform.worldPosition).length();
		}
		transform.needUpdate = false;
	}
	camera.needUpdate = false;
}

void RenderSystem::draw(){

	//---------Draw all meshes----------
	auto meshes = scene->getComponentArray<MeshComponent>();


	//---------Depth shader----------
	auto lights = scene->getComponentArray<LightComponent>();
	LightComponent& light = lights->getComponentFromIndex(0);
	Entity entity = lights->getEntity(0);
	Transform* transform = scene->findComponent<Transform>(entity);

	depthRender.startFrameBuffer(&light.lightFb);
	//depthRender.startDefaultFrameBuffer(System::instance().getScreenWidth(), System::instance().getScreenHeight());
	
	for (int i = 0; i < meshes->size(); i++){
		MeshComponent& mesh = meshes->getComponentFromIndex(i);
		Entity entity = meshes->getEntity(i);
		Transform* transform = scene->findComponent<Transform>(entity);

		if (transform){
			if (!mesh.loaded){
				loadMesh(mesh, light.lightFb);
			}
			drawMeshDepth(mesh, light.lightViewProjection * transform->modelMatrix);
		}
	}

	depthRender.endFrameBuffer();
	
	//---------Draw transparent meshes----------
	sceneRender.startDefaultFrameBuffer(System::instance().getScreenWidth(), System::instance().getScreenHeight());
	sceneRender.applyViewport(Engine::getViewRect());

	u_lighting_t lightsFormated = collectLights();

	Transform& cameraTransform =  scene->getComponent<Transform>(scene->getCamera());
	
	for (int i = 0; i < meshes->size(); i++){
		MeshComponent& mesh = meshes->getComponentFromIndex(i);
		Entity entity = meshes->getEntity(i);
		Transform* transform = scene->findComponent<Transform>(entity);

		if (transform){
			if (!mesh.loaded){
				loadMesh(mesh, light.lightFb);
			}
			if (!mesh.transparency){
				//Draw opaque meshes
				drawMesh(mesh, *transform, cameraTransform, lightsFormated, light.lightViewProjection * transform->modelMatrix);
			}else{
				transparentMeshes.push({&mesh, transform, transform->distanceToCamera});
			}
		}
	}

	//---------Draw sky----------
	SkyComponent* sky = scene->findComponentFromIndex<SkyComponent>(0);
	if (sky){
		if (!sky->loaded){
			loadSky(*sky);
		}

		drawSky(*sky);
	}

	//---------Draw opaque meshes----------
	while (!transparentMeshes.empty()){
		TransparentMeshesData meshData = transparentMeshes.top();

		//Draw transparent meshes
		drawMesh(*meshData.mesh, *meshData.transform, cameraTransform, lightsFormated, light.lightViewProjection * transform->modelMatrix);

		transparentMeshes.pop();
	}

	sceneRender.endFrameBuffer();

	//---------Missing some shaders----------
	if (ShaderPool::getMissingShaders().size() > 0){
		std::string misShaders;
		for (int i = 0; i < ShaderPool::getMissingShaders().size(); i++){
			if (!misShaders.empty())
				misShaders += "; ";
			misShaders += ShaderPool::getMissingShaders()[i];
		}
		Log::Verbose(
			"\n"
			"-------------------\n"
			"Supernova is missing some shaders, you need to use Supershader tool to create these shaders in project assets directory.\n"
			"Go to directory \"tools/supershader\" and execute the command:\n"
			"\n"
			"> python supershader.py -s \"%s\" -p \"%s\" -l %s\n"
			"-------------------"
			, misShaders.c_str(), PROJECT_ROOT, ShaderPool::getShaderLangStr().c_str());
		exit(1);
	}
	
	SystemRender::commit();
}

void RenderSystem::entityDestroyed(Entity entity){
	auto mesh = scene->findComponent<MeshComponent>(entity);
	
	if (mesh){
		for (int i = 0; i < mesh->numSubmeshes; i++){

			Submesh& submesh = mesh->submeshes[i];

			//Destroy shader
			submesh.shader.reset();
			ShaderPool::remove(ShaderType::MESH, mesh->submeshes[i].shaderProperties);
			if (hasLights && mesh->castShadows)
				ShaderPool::remove(ShaderType::DEPTH, "");

			//Destroy texture
			submesh.material.baseColorTexture.destroy();

			//Destroy render
			submesh.render.destroy();
		}

		//Destroy buffers
		for (auto const& buf : mesh->buffers){
			buf.second->getRender()->destroyBuffer();
		}
	}

	auto sky = scene->findComponent<SkyComponent>(entity);
	if (sky){
		//Destroy shader
		sky->shader.reset();
		ShaderPool::remove(sky->shaderType, "");

		//Destroy texture
		sky->texture.destroy();

		//Destroy render
		sky->render.destroy();

		//Destroy buffer
		if (sky->buffer){
			sky->buffer->getRender()->destroyBuffer();
		}
	}

}
