/* Header for the image processing class */

#ifndef __IMAGE
#define __IMAGE

#include "Pixel.h"
#include "Picture.h"

namespace DSP
{
	class Image
	{
	public:
		Picture edge_detection(const Picture&);
		Picture flip(const Picture&);
	private:
		void copy_ppixel_to_picture(Picture&, const ProcessedPixels&);
		ProcessedPixels convert_to_grayscale(const Picture& picture);
		void write_grayscale_bmp(char *bmp, byte *header, Pixel *data, int width, int height);
		ProcessedPixels gaussian_blur(const ProcessedPixels& picture);
		ProcessedPixels sobel_filter(const ProcessedPixels& picture, ProcessedPixels& phase);
		int compute_phase(double x, double y);
		ProcessedPixels non_maximum_suppressor(const ProcessedPixels& picture, const ProcessedPixels& grad_theta);
		ProcessedPixels hysteresis_filter(const ProcessedPixels& picture);
	};

	namespace algebra
	{
		void convolution(int* kernel, int kRows, int kCols, const ProcessedPixels& picture,
						 double scaling_factor, ProcessedPixels& p_pixels);
	}
}


#endif //__IMAGE
