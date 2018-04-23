#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include "GUIImage.h"
#include "Text.h"

namespace Supernova {

    class TextEdit: public GUIImage {
    private:

        Text text;

    public:
        TextEdit();
        virtual ~TextEdit();

        void setText(std::string text);
        void setTextFont(std::string font);
        void setTextSize(unsigned int size);
        void setTextColor(Vector4 color);
        Text* getTextObject();

        virtual void engine_onPress(float x, float y);
        virtual void engine_onUp(float x, float y);
        virtual void engine_onTextInput(const char* text);

        virtual bool load();
    };

}


#endif //TEXTEDIT_H
