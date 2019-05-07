/* Declaration of the pixel data structures and ProcessedPixels class 
 * Ordering of pixel colors in BMP files is b, g, r
 */

#ifndef __PIXEL
#define __PIXEL

#include "Shared_Ptr.h"

typedef char byte;

struct Pixel 
{
   	byte b;
   	byte g;
   	byte r;
};

// Intermediate results of processing steps can
// generate values that do not fit a byte. They
// are not  meaningful as  "image data", but as
// intermediate values to reach the final image
// result.
struct PPixel
{
	double b;
	double g;
	double r;
};

// A RAII class for dealing with PPixel
class ProcessedPixels
{
public:
	ProcessedPixels(int width, int height);
	ProcessedPixels(const ProcessedPixels &other); 
	~ProcessedPixels();

	ProcessedPixels& operator= (const ProcessedPixels& other);

	PPixel* get_pixels(void) const {return p_pixel.release_ptr();}
	int get_width() const { return width; }
	int get_height() const { return height; }

private:
	Shared_ptr<PPixel> p_pixel;
	int width;
	int height;

	void do_copy(ProcessedPixels& current, const ProcessedPixels& other);
};

#endif //__PIXEL
