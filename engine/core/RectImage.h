#ifndef RectImage_h
#define RectImage_h

#include "Image.h"
#include "Rect.h"


class RectImage: public Image {
private:
    int texWidth;
    int texHeight;
    
    bool useTextureRect;

    Rect textureRect;
    
protected:

    void normalizeTextureRect();
    
public:
    RectImage();
    virtual ~RectImage();

    void setRect(float x, float y, float width, float height);
    void setRect(Rect textureRect);
    
    bool load();
    bool draw();
};

#endif /* RectImage_h */
