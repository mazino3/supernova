//
// (c) 2021 Eduardo Doria.
//

#ifndef UISYSTEM_H
#define UISYSTEM_H

#include "SubSystem.h"

#include "component/UIRenderComponent.h"
#include "component/TextComponent.h"
#include "component/ImageComponent.h"

namespace Supernova{

	class UISystem : public SubSystem {

    private:

		//Image
		bool createImagePatches(ImageComponent& img, UIRenderComponent& ui);

		// Text
		bool loadFontAtlas(TextComponent& text, UIRenderComponent& ui);
		void createText(TextComponent& text, UIRenderComponent& ui);

	public:
		UISystem(Scene* scene);

		virtual void load();
		virtual void draw();
		virtual void update(double dt);

		virtual void entityDestroyed(Entity entity);
	};

}

#endif //UISYSTEM_H