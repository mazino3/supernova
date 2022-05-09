//
// (c) 2021 Eduardo Doria.
//

#include "ActionSystem.h"

#include "Scene.h"
#include "math/Color.h"
#include "math/Angle.h"

using namespace Supernova;


ActionSystem::ActionSystem(Scene* scene): SubSystem(scene){
    signature.set(scene->getComponentType<ActionComponent>());
}

void ActionSystem::actionStart(ActionComponent& action){
    action.onStart.call();
}

void ActionSystem::actionStop(ActionComponent& action){
    action.onStop.call();
}

void ActionSystem::actionPause(ActionComponent& action){
    action.onPause.call();
}

void ActionSystem::setSpriteTextureRect(MeshComponent& mesh, SpriteComponent& sprite, SpriteAnimationComponent& spriteanim){
    if (spriteanim.frameIndex < MAX_SPRITE_FRAMES){
        FrameData frameData = sprite.framesRect[spriteanim.frames[spriteanim.frameIndex]];
        if (frameData.active)
            mesh.submeshes[0].textureRect = frameData.rect;
    }
}

void ActionSystem::spriteActionStart(MeshComponent& mesh, SpriteComponent& sprite, SpriteAnimationComponent& spriteanim){
    setSpriteTextureRect(mesh, sprite, spriteanim);
}

void ActionSystem::spriteActionStop(MeshComponent& mesh, SpriteComponent& sprite, SpriteAnimationComponent& spriteanim){
    spriteanim.frameIndex = 0;
    spriteanim.frameTimeIndex = 0;
    spriteanim.spriteFrameCount = 0;

    setSpriteTextureRect(mesh, sprite, spriteanim);
}

void ActionSystem::spriteActionUpdate(double dt, ActionComponent& action, MeshComponent& mesh, SpriteComponent& sprite, SpriteAnimationComponent& spriteanim){
    spriteanim.spriteFrameCount += dt * 1000;
    while (spriteanim.spriteFrameCount >= spriteanim.framesTime[spriteanim.frameTimeIndex]) {

        spriteanim.spriteFrameCount -= spriteanim.framesTime[spriteanim.frameTimeIndex];

        spriteanim.frameIndex++;
        spriteanim.frameTimeIndex++;

        if (spriteanim.frameIndex == spriteanim.framesSize - 1) {
            if (!spriteanim.loop) {
                action.stopTrigger = true;
            }
        }

        if (spriteanim.frameIndex >= spriteanim.framesSize)
            spriteanim.frameIndex = 0;
        
        if (spriteanim.frameTimeIndex >= spriteanim.framesTimeSize)
            spriteanim.frameTimeIndex = 0;

        setSpriteTextureRect(mesh, sprite, spriteanim);
    }
}

void ActionSystem::timedActionStop(TimedActionComponent& timedaction){
    timedaction.timecount = 0;
    timedaction.time = 0;
    timedaction.value = 0;
}

void ActionSystem::timedActionUpdate(double dt, ActionComponent& action, TimedActionComponent& timedaction){
    timedaction.timecount += dt;

    if ((timedaction.time == 1) && !timedaction.loop){
        action.stopTrigger = true;
        //onFinish.call(object);
    } else {
        if (timedaction.duration >= 0) {

            if (timedaction.timecount >= timedaction.duration){
                if (!timedaction.loop){
                    timedaction.timecount = timedaction.duration;
                }else{
                    timedaction.timecount -= timedaction.duration;
                }
            }

            timedaction.time = timedaction.timecount / timedaction.duration;
        }

        timedaction.value = timedaction.function.call(timedaction.time);
        //Log::Debug("step time %f value %f \n", timedaction.time, timedaction.value);
    }
}

void ActionSystem::positionActionUpdate(double dt, ActionComponent& action, TimedActionComponent& timedaction, PositionActionComponent& posaction, Transform& transform){
    Vector3 position = (posaction.endPosition - posaction.startPosition) * timedaction.value;
    transform.position = posaction.startPosition + position;
    transform.needUpdate = true;
}

void ActionSystem::rotationActionUpdate(double dt, ActionComponent& action, TimedActionComponent& timedaction, RotationActionComponent& rotaction, Transform& transform){
    transform.rotation = transform.rotation.slerp(timedaction.value, rotaction.startRotation, rotaction.endRotation);
    transform.needUpdate = true;
}

void ActionSystem::scaleActionUpdate(double dt, ActionComponent& action, TimedActionComponent& timedaction, ScaleActionComponent& scaleaction, Transform& transform){
    Vector3 scale = (scaleaction.endScale - scaleaction.startScale) * timedaction.value;
    transform.scale = scaleaction.startScale + scale;
    transform.needUpdate = true;
}

void ActionSystem::colorActionMeshUpdate(double dt, ActionComponent& action, TimedActionComponent& timedaction, ColorActionComponent& coloraction, MeshComponent& mesh){
    Vector3 color = (coloraction.endColor - coloraction.startColor) * timedaction.value;
    if (coloraction.useSRGB){
        mesh.submeshes[0].material.baseColorFactor = Color::sRGBToLinear(coloraction.startColor + color);
    }else{
        mesh.submeshes[0].material.baseColorFactor = coloraction.startColor + color;
    }
}

void ActionSystem::colorActionUIUpdate(double dt, ActionComponent& action, TimedActionComponent& timedaction, ColorActionComponent& coloraction, UIComponent& uirender){
    Vector3 color = (coloraction.endColor - coloraction.startColor) * timedaction.value;
    if (coloraction.useSRGB){
        uirender.color = Color::sRGBToLinear(coloraction.startColor + color);
    }else{
        uirender.color = coloraction.startColor + color;
    }
}

void ActionSystem::alphaActionMeshUpdate(double dt, ActionComponent& action, TimedActionComponent& timedaction, AlphaActionComponent& alphaaction, MeshComponent& mesh){
    float alpha = (alphaaction.endAlpha - alphaaction.startAlpha) * timedaction.value;

    mesh.submeshes[0].material.baseColorFactor.w = alphaaction.startAlpha + alpha;
}

void ActionSystem::alphaActionUIUpdate(double dt, ActionComponent& action, TimedActionComponent& timedaction, AlphaActionComponent& alphaaction, UIComponent& uirender){
    float alpha = (alphaaction.endAlpha - alphaaction.startAlpha) * timedaction.value;

    uirender.color.w = alphaaction.startAlpha + alpha;
}

int ActionSystem::findUnusedParticle(ParticlesComponent& particles, ParticlesAnimationComponent& partanim){

    for (int i=partanim.lastUsedParticle; i<particles.particles.size(); i++){
        if (particles.particles[i].life <= particles.particles[i].time){
            partanim.lastUsedParticle = i;
            return i;
        }
    }

    for (int i=0; i<partanim.lastUsedParticle; i++){
        if (particles.particles[i].life <= particles.particles[i].time){
            partanim.lastUsedParticle = i;
            return i;
        }
    }

    return -1;
}

float ActionSystem::getFloatInitializerValue(float& min, float& max){
    if (min != max) {
        return min + ((max - min) * (float) rand() / (float) RAND_MAX);
    }
    return max;
}

Vector3 ActionSystem::getVector3InitializerValue(Vector3& min, Vector3& max){
    if (min != max) {
        return Vector3( min.x + ((max.x - min.x) * (float) rand() / (float) RAND_MAX),
                        min.y + ((max.y - min.y) * (float) rand() / (float) RAND_MAX),
                        min.z + ((max.z - min.z) * (float) rand() / (float) RAND_MAX));
    }
    return max;
}

Rect ActionSystem::getSpriteInitializerValue(std::vector<int>& frames, ParticlesComponent& particles){
    if (frames.size() > 0){
        int id = frames[int(frames.size()*rand()/(RAND_MAX + 1.0))];

        if (id >= 0 && id < MAX_SPRITE_FRAMES && particles.framesRect[id].active){
            return particles.framesRect[id].rect;
        }
    }

    return Rect(0,0,1,1);
}

void ActionSystem::applyParticleInitializers(size_t idx, ParticlesComponent& particles, ParticlesAnimationComponent& partanim){

    ParticleLifeInitializer& lifeInit = partanim.lifeInitializer;
    particles.particles[idx].life = getFloatInitializerValue(lifeInit.minLife, lifeInit.maxLife);

    ParticlePositionInitializer& posInit = partanim.positionInitializer;
    particles.particles[idx].position = getVector3InitializerValue(posInit.minPosition, posInit.maxPosition);

    ParticleVelocityInitializer& velInit = partanim.velocityInitializer;
    particles.particles[idx].velocity = getVector3InitializerValue(velInit.minVelocity, velInit.maxVelocity);

    ParticleAccelerationInitializer& accInit = partanim.accelerationInitializer;
    particles.particles[idx].acceleration = getVector3InitializerValue(accInit.minAcceleration, accInit.maxAcceleration);

    ParticleColorInitializer& colInit = partanim.colorInitializer;
    particles.particles[idx].color = getVector3InitializerValue(colInit.minColor, colInit.maxColor);
    if (partanim.colorInitializer.useSRGB){
        particles.particles[idx].color = Color::sRGBToLinear(particles.particles[idx].color);
    }

    ParticleAlphaInitializer& alpInit = partanim.alphaInitializer;
    particles.particles[idx].color.w = getFloatInitializerValue(alpInit.minAlpha, alpInit.maxAlpha);

    ParticleSizeInitializer& sizeInit = partanim.sizeInitializer;
    particles.particles[idx].size = getFloatInitializerValue(sizeInit.minSize, sizeInit.maxSize);

    ParticleSpriteInitializer& spriteInit = partanim.spriteInitializer;
    particles.particles[idx].textureRect = getSpriteInitializerValue(spriteInit.frames, particles);

    ParticleRotationInitializer& rotInit = partanim.rotationInitializer;
    particles.particles[idx].rotation = Angle::defaultToRad(getFloatInitializerValue(rotInit.minRotation, rotInit.maxRotation));

}

float ActionSystem::getTimeFromParticleTime(float& time, float& fromTime, float& toTime){
    if ((fromTime != toTime) && (time >= fromTime) && (time <= toTime)) {
        return (time - fromTime) / (toTime - fromTime);
    }

    return -1;
}

float ActionSystem::getFloatModifierValue(float& value, float& fromValue, float& toValue){
    return fromValue + ((toValue - fromValue) * value);
}

Vector3 ActionSystem::getVector3ModifierValue(float& value, Vector3& fromValue, Vector3& toValue){
    return fromValue + ((toValue - fromValue) * value);
}

Rect ActionSystem::getSpriteModifierValue(float& value, std::vector<int>& frames, ParticlesComponent& particles){
    if (frames.size() > 0){
        int id = frames[(int)(frames.size() * value)];

        if (id >= 0 && id < MAX_SPRITE_FRAMES && particles.framesRect[id].active){
            return particles.framesRect[id].rect;
        }
    }

    return Rect(0,0,1,1);
}

void ActionSystem::applyParticleModifiers(size_t idx, ParticlesComponent& particles, ParticlesAnimationComponent& partanim){

    float particleTime = particles.particles[idx].time;
    float value;
    float time;

    ParticlePositionModifier& posMod = partanim.positionModifier;
    time = getTimeFromParticleTime(particleTime, posMod.fromTime, posMod.toTime);
    value = posMod.function.call(time);
    if (value >= 0 && value <= 1){
        particles.particles[idx].position = getVector3ModifierValue(value, posMod.fromPosition, posMod.toPosition);
    }

    ParticleVelocityModifier& velMod = partanim.velocityModifier;
    time = getTimeFromParticleTime(particleTime, velMod.fromTime, velMod.toTime);
    value = velMod.function.call(time);
    if (value >= 0 && value <= 1){
        particles.particles[idx].velocity = getVector3ModifierValue(value, velMod.fromVelocity, velMod.toVelocity);
    }

    ParticleAccelerationModifier& accMod = partanim.accelerationModifier;
    time = getTimeFromParticleTime(particleTime, accMod.fromTime, accMod.toTime);
    value = accMod.function.call(time);
    if (value >= 0 && value <= 1){
        particles.particles[idx].acceleration = getVector3ModifierValue(value, accMod.fromAcceleration, accMod.toAcceleration);
    }

    ParticleColorModifier& colMod = partanim.colorModifier;
    time = getTimeFromParticleTime(particleTime, colMod.fromTime, colMod.toTime);
    value = colMod.function.call(time);
    if (value >= 0 && value <= 1){
        particles.particles[idx].color = getVector3ModifierValue(value, colMod.fromColor, colMod.toColor);
        if (partanim.colorModifier.useSRGB){
            particles.particles[idx].color = Color::sRGBToLinear(particles.particles[idx].color);
        }
    }

    ParticleAlphaModifier& alpMod = partanim.alphaModifier;
    time = getTimeFromParticleTime(particleTime, alpMod.fromTime, alpMod.toTime);
    value = alpMod.function.call(time);
    if (value >= 0 && value <= 1){
        particles.particles[idx].color.w = getFloatModifierValue(value, alpMod.fromAlpha, alpMod.toAlpha);
    }

    ParticleSizeModifier& sizeMod = partanim.sizeModifier;
    time = getTimeFromParticleTime(particleTime, sizeMod.fromTime, sizeMod.toTime);
    value = sizeMod.function.call(time);
    if (value >= 0 && value <= 1){
        particles.particles[idx].size = getFloatModifierValue(value, sizeMod.fromSize, sizeMod.toSize);
    }

    ParticleSpriteModifier& spriteMod = partanim.spriteModifier;
    time = getTimeFromParticleTime(particleTime, spriteMod.fromTime, spriteMod.toTime);
    value = spriteMod.function.call(time);
    if (value >= 0 && value <= 1){
        particles.particles[idx].textureRect = getSpriteModifierValue(value, spriteMod.frames, particles);
    }

    ParticleRotationModifier& rotMod = partanim.rotationModifier;
    time = getTimeFromParticleTime(particleTime, rotMod.fromTime, rotMod.toTime);
    value = rotMod.function.call(time);
    if (value >= 0 && value <= 1){
        particles.particles[idx].rotation = Angle::defaultToRad(getFloatModifierValue(value, rotMod.fromRotation, rotMod.toRotation));
    }
}

void ActionSystem::particleActionStart(ParticlesAnimationComponent& partanim, ParticlesComponent& particles){
    // Creating particles
    particles.particles.clear();
    for (int i = 0; i < particles.maxParticles; i++){
        particles.particles.push_back({});
        particles.particles.back().life = 0;
        particles.particles.back().time = 0;
    }

    partanim.emitter = true;
}

void ActionSystem::particlesActionUpdate(double dt, Entity entity, ActionComponent& action, ParticlesAnimationComponent& partanim, ParticlesComponent& particles){
    if (partanim.emitter){
        partanim.newParticlesCount += dt * partanim.rate;

        int newparticles = (int)partanim.newParticlesCount;
        partanim.newParticlesCount -= newparticles;
        if (newparticles > partanim.maxPerUpdate)
            newparticles = partanim.maxPerUpdate;

        for(int i=0; i<newparticles; i++){
            int particleIndex = findUnusedParticle(particles, partanim);

            if (particleIndex >= 0){

                particles.particles[particleIndex].time = 0;

                applyParticleInitializers(particleIndex, particles, partanim);

                particles.needUpdate = true;
            }

        }
    }

    bool existParticles = false;
    for(int i=0; i<particles.particles.size(); i++){

        float life = particles.particles[i].life;
        float time = particles.particles[i].time;

        if(life > time){

            applyParticleModifiers(i, particles, partanim);

            Vector3 velocity = particles.particles[i].velocity;
            Vector3 position = particles.particles[i].position;
            Vector3 acceleration = particles.particles[i].acceleration;

            velocity += acceleration * dt * 0.5f;
            position += velocity * dt;
            time += dt;

            particles.particles[i].time = time;
            particles.particles[i].velocity = velocity;
            particles.particles[i].position = position;

            existParticles = true;

            particles.needUpdate = true;

            //printf("1.Particle %i life %f time %f position %f %f %f\n", i, life, time, position.x, position.y, position.z);
        }
    }

    if (!existParticles && !partanim.emitter){
        action.stopTrigger = true;
        //onFinish.call(object);
    }
}

void ActionSystem::load(){

}

void ActionSystem::destroy(){

}

void ActionSystem::draw(){

}

void ActionSystem::update(double dt){
    auto actions = scene->getComponentArray<ActionComponent>();
    for (int i = 0; i < actions->size(); i++){
		ActionComponent& action = actions->getComponentFromIndex(i);

        // Action start
		if (action.startTrigger == true && (action.state == ActionState::Stopped || action.state == ActionState::Paused)){
            action.state = ActionState::Running;
            action.startTrigger = false;
            actionStart(action);

            Entity entity = actions->getEntity(i);
            Signature targetSignature = scene->getSignature(action.target);
            Signature signature = scene->getSignature(entity);

            if (signature.test(scene->getComponentType<SpriteAnimationComponent>())){
                SpriteAnimationComponent& spriteanim = scene->getComponent<SpriteAnimationComponent>(entity);
                if (targetSignature.test(scene->getComponentType<SpriteComponent>()) && targetSignature.test(scene->getComponentType<MeshComponent>())){
                    SpriteComponent& sprite = scene->getComponent<SpriteComponent>(action.target);
                    MeshComponent& mesh = scene->getComponent<MeshComponent>(action.target);

                    spriteActionStart(mesh, sprite, spriteanim);

                }
            }

            if (signature.test(scene->getComponentType<ParticlesAnimationComponent>())){
                ParticlesAnimationComponent& partanim = scene->getComponent<ParticlesAnimationComponent>(entity);
                if (targetSignature.test(scene->getComponentType<ParticlesComponent>()) ){
                    ParticlesComponent& particles = scene->getComponent<ParticlesComponent>(action.target);

                    particleActionStart(partanim, particles);

                }
            }
        }

        // Action stop
		if (action.stopTrigger == true && (action.state == ActionState::Running || action.state == ActionState::Paused)){
            action.state = ActionState::Stopped;
            action.stopTrigger = false;
            actionStop(action);

            Entity entity = actions->getEntity(i);
            Signature targetSignature = scene->getSignature(action.target);
            Signature signature = scene->getSignature(entity);

            if (signature.test(scene->getComponentType<SpriteAnimationComponent>())){
                SpriteAnimationComponent& spriteanim = scene->getComponent<SpriteAnimationComponent>(entity);
                if (targetSignature.test(scene->getComponentType<SpriteComponent>()) && targetSignature.test(scene->getComponentType<MeshComponent>())){
                    SpriteComponent& sprite = scene->getComponent<SpriteComponent>(action.target);
                    MeshComponent& mesh = scene->getComponent<MeshComponent>(action.target);

                    spriteActionStop(mesh, sprite, spriteanim);

                }
            }

            if (signature.test(scene->getComponentType<TimedActionComponent>())){
                TimedActionComponent& timedaction = scene->getComponent<TimedActionComponent>(entity);

                timedActionStop(timedaction);
            }
        }

        // Action pause
        if (action.pauseTrigger == true && action.state == ActionState::Running){
            action.state = ActionState::Paused;
            action.pauseTrigger = false;
            actionPause(action);
        }

        // Action update
        if ((action.state == ActionState::Running) && (action.target != NULL_ENTITY)){

            Entity entity = actions->getEntity(i);
            Signature targetSignature = scene->getSignature(action.target);
            Signature signature = scene->getSignature(entity);

            //Sprite animation
            if (signature.test(scene->getComponentType<SpriteAnimationComponent>())){
                SpriteAnimationComponent& spriteanim = scene->getComponent<SpriteAnimationComponent>(entity);
                if (targetSignature.test(scene->getComponentType<SpriteComponent>()) && targetSignature.test(scene->getComponentType<MeshComponent>())){
                    SpriteComponent& sprite = scene->getComponent<SpriteComponent>(action.target);
                    MeshComponent& mesh = scene->getComponent<MeshComponent>(action.target);

                    spriteActionUpdate(dt, action, mesh, sprite, spriteanim);

                }
            }

            //Particles
            if (signature.test(scene->getComponentType<ParticlesAnimationComponent>())){
                ParticlesAnimationComponent& partanim = scene->getComponent<ParticlesAnimationComponent>(entity);

                if (targetSignature.test(scene->getComponentType<ParticlesComponent>())){
                    ParticlesComponent& particles = scene->getComponent<ParticlesComponent>(action.target);

                    particlesActionUpdate(dt, entity, action, partanim, particles);
                }
            }

            if (signature.test(scene->getComponentType<TimedActionComponent>())){
                TimedActionComponent& timedaction = scene->getComponent<TimedActionComponent>(entity);

                timedActionUpdate(dt, action, timedaction);

                //Transform animation
                if (targetSignature.test(scene->getComponentType<Transform>())){
                    Transform& transform = scene->getComponent<Transform>(action.target);

                    if (signature.test(scene->getComponentType<PositionActionComponent>())){
                        PositionActionComponent& posaction = scene->getComponent<PositionActionComponent>(entity);

                        positionActionUpdate(dt, action, timedaction, posaction, transform);
                    }

                    if (signature.test(scene->getComponentType<RotationActionComponent>())){
                        RotationActionComponent& rotaction = scene->getComponent<RotationActionComponent>(entity);

                        rotationActionUpdate(dt, action, timedaction, rotaction, transform);
                    }

                    if (signature.test(scene->getComponentType<ScaleActionComponent>())){
                        ScaleActionComponent& scaleaction = scene->getComponent<ScaleActionComponent>(entity);

                        scaleActionUpdate(dt, action, timedaction, scaleaction, transform);
                    }
                }

                //Color animation
                if (signature.test(scene->getComponentType<ColorActionComponent>())){
                    ColorActionComponent& coloraction = scene->getComponent<ColorActionComponent>(entity);

                    if (targetSignature.test(scene->getComponentType<MeshComponent>())){
                        MeshComponent& mesh = scene->getComponent<MeshComponent>(action.target);

                        colorActionMeshUpdate(dt, action, timedaction, coloraction, mesh);
                    }
                    if (targetSignature.test(scene->getComponentType<UIComponent>())){
                        UIComponent& uirender = scene->getComponent<UIComponent>(action.target);

                        colorActionUIUpdate(dt, action, timedaction, coloraction, uirender);
                    }
                }

                //Alpha animation
                if (signature.test(scene->getComponentType<AlphaActionComponent>())){
                    AlphaActionComponent& alphaaction = scene->getComponent<AlphaActionComponent>(entity);

                    if (targetSignature.test(scene->getComponentType<MeshComponent>())){
                        MeshComponent& mesh = scene->getComponent<MeshComponent>(action.target);

                        alphaActionMeshUpdate(dt, action, timedaction, alphaaction, mesh);
                    }
                    if (targetSignature.test(scene->getComponentType<UIComponent>())){
                        UIComponent& uirender = scene->getComponent<UIComponent>(action.target);

                        alphaActionUIUpdate(dt, action, timedaction, alphaaction, uirender);
                    }
                }
            }
        }

	}
}

void ActionSystem::entityDestroyed(Entity entity){

}