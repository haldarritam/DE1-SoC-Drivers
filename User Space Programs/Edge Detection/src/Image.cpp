/* Definitions for the image processing class */

#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include "Image.h"
#include "Pixel.h"
#include "Shared_Ptr.h"

using namespace DSP;

void Image::copy_ppixel_to_picture(Picture& picture, const ProcessedPixels& p_pixels)
{
	for(int i = 0; i < picture.get_width() * picture.get_height(); i++)
	{
		picture.get_pixels()[i].r = p_pixels.get_pixels()[i].r;
		picture.get_pixels()[i].g = p_pixels.get_pixels()[i].g;
		picture.get_pixels()[i].b = p_pixels.get_pixels()[i].b;
	}
}

Picture Image::flip(const Picture& picture)
{
	int t_col = picture.get_width();
	int t_row = picture.get_height();
	Shared_ptr<Pixel> flipped_data(t_row * t_col);

	for (int row = t_row-1, flip_row = 0; row >= 0; row--, flip_row++)
		for(int col = 0; col < t_col; col++)
			flipped_data[flip_row*t_col+col] = picture.get_pixels()[row*t_col+col];

	return Picture(flipped_data, picture.get_header(), t_col, t_row, picture.get_file_name());
}

// Determine the grayscale 8 bit value by averaging the r, g, and b channel values.
// Store the 8 bit grayscale value in the r channel.
ProcessedPixels Image::convert_to_grayscale(const Picture& picture) 
{
   const float num_colors = 3;
   int width = picture.get_width();
   int height = picture.get_height();
   ProcessedPixels gs_picture(width, height);
   
   for (int y = 0; y < height; y++)
      for (int x = 0; x < width; x++) 
	  {
      	// Just use the 8 bits in the r field to hold the entire grayscale image
         (*(gs_picture.get_pixels() + y*width + x)).r = ((*(picture.get_pixels() + y*width + x)).r + 
			(*(picture.get_pixels() + y*width + x)).g + (*(picture.get_pixels() + y*width + x)).b) / num_colors;

		// And copy the grayscale pixels to the other channels
		(*(gs_picture.get_pixels() + y*width + x)).g = (*(gs_picture.get_pixels() + y*width + x)).r;
		(*(gs_picture.get_pixels() + y*width + x)).b = (*(gs_picture.get_pixels() + y*width + x)).r;
      }

	return gs_picture;
}

// Write the grayscale image to disk. The 8-bit grayscale values should be inside the
// r channel of each pixel.
void Image::write_grayscale_bmp(char *bmp, byte *header, Pixel *data, int width, int height) {
   FILE* file = fopen (bmp, "wb");
   
   // write the 54-byte header
   fwrite (header, sizeof(byte), Picture::header_size, file); 
   int y, x;
   
   // the r field of the pixel has the grayscale value. Copy to g and b.
   for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
         (*(data + y*width + x)).b = (*(data + y*width + x)).r;
         (*(data + y*width + x)).g = (*(data + y*width + x)).r;
      }
   }
   int size = width * height;
   fwrite (data, sizeof(Pixel), size, file); // write the rest of the data
   fclose (file);
}

// Gaussian blur. Operate on the .r fields of the pixels only.
ProcessedPixels Image::gaussian_blur(const ProcessedPixels& picture)
{
   const double scaling_factor = 159.0;
   const int filter_size = 5;
   int gaussian_filter[filter_size][filter_size] = {
      { 2, 4, 5, 4, 2 },
      { 4, 9,12, 9, 4 },
      { 5,12,15,12, 5 },
      { 4, 9,12, 9, 4 },
      { 2, 4, 5, 4, 2 }
   };

	ProcessedPixels p_pixels(picture.get_width(), picture.get_height());
	algebra::convolution(&gaussian_filter[0][0], filter_size, filter_size,
						 picture, scaling_factor, p_pixels);

	return p_pixels;
}

ProcessedPixels Image::sobel_filter(const ProcessedPixels& picture,
									ProcessedPixels& phase)
{
	const double scaling_factor = 1;
	const int filter_size = 3;

   // Definition of Sobel filter in horizontal and veritcal directions
   int horizontal_operator[filter_size][filter_size] = {
      { -1,  0,  1 },
      { -2,  0,  2 },
      { -1,  0,  1 }
   };
   int vertical_operator[filter_size][filter_size] = {
      { -1,  -2,  -1 },
      {  0,   0,   0 },
      {  1,   2,   1 }
   };

	ProcessedPixels processed_picture = picture;
	ProcessedPixels cx(picture.get_width(), picture.get_height());
	ProcessedPixels cy(picture.get_width(), picture.get_height());
	algebra::convolution(&horizontal_operator[0][0], filter_size,
						 filter_size, picture, scaling_factor, cx);
	algebra::convolution(&vertical_operator[0][0], filter_size,
						 filter_size, picture, scaling_factor, cy);
	
	for (int i = 0; i < (picture.get_width() * picture.get_height()); i++)
	{
		const int avg_size = 2;

		double current_pixel = (abs(cx.get_pixels()[i].r) / avg_size) +
							   (abs(cy.get_pixels()[i].r) / avg_size);

		int current_phase = compute_phase(cx.get_pixels()[i].r,
										  cy.get_pixels()[i].r);
							
		processed_picture.get_pixels()[i].r = current_pixel;
		processed_picture.get_pixels()[i].g = current_pixel;
		processed_picture.get_pixels()[i].b = current_pixel;

		phase.get_pixels()[i].r = current_phase;
		phase.get_pixels()[i].g = current_phase;
		phase.get_pixels()[i].b = current_phase;
	}

	return processed_picture;
}

int Image::compute_phase(double x, double y)
{
	const int num_clusters = 5;
	const int cluster[num_clusters] = {0,45,90,135,180};
	const int wrapper_angle = 180;
	const float rad_to_deg_factor = 180.0 / M_PI;

	int temp_theta = atan2(y, x) * rad_to_deg_factor;
	if (temp_theta < 0)
		temp_theta += wrapper_angle;

	int lowest = 0;

	// starting angle clusterization
	for (int i = 0; i < num_clusters; i++)
	{
		int distance = pow(temp_theta - cluster[i], 2);
		int lowest_distance = pow(temp_theta - cluster[lowest], 2);
		if (lowest_distance > distance)
			lowest = i;
	} 			

	return (cluster[lowest] == 180) ? cluster[0] : cluster[lowest];
}

ProcessedPixels Image::non_maximum_suppressor(const ProcessedPixels& picture, const ProcessedPixels& grad_theta)
{
	ProcessedPixels processed_picture = picture;
	int height = picture.get_height();
	int width = picture.get_width();
	
	for (int i = 1; i < (height - 1); i++)
	{
		for (int j = 1; j < (width - 1); j++)
		{
			int index = i * width + j;
			int index_w = i * width + (j-1);
			int index_e = i * width + (j+1);
			int index_s = (i+1) * width + j;
			int index_n = (i-1) * width + j;
			int index_nw = (i-1) * width + (j-1);
			int index_se = (i+1) * width + (j+1);
			int index_ne = (i-1) * width + (j+1);
			int index_sw = (i+1) * width + (j-1);
			
			switch(static_cast<int>(grad_theta.get_pixels()[index].r))
			{
				case(0):
					if ((picture.get_pixels()[index].r <= 
						picture.get_pixels()[index_w].r) ||
						(picture.get_pixels()[index].r <= 
						picture.get_pixels()[index_e].r))
						processed_picture.get_pixels()[index].r = 0;
					break;
				case(90):
					if ((picture.get_pixels()[index].r <= 
						picture.get_pixels()[index_s].r) ||
						(picture.get_pixels()[index].r <= 
						picture.get_pixels()[index_n].r))
						processed_picture.get_pixels()[index].r = 0;
					break;
				case(135):
					if ((picture.get_pixels()[index].r <= 
						picture.get_pixels()[index_ne].r) ||
						(picture.get_pixels()[index].r <= 
						picture.get_pixels()[index_sw].r))
						processed_picture.get_pixels()[index].r = 0;
					break;
				case(45):
					if ((picture.get_pixels()[index].r <= 
						picture.get_pixels()[index_nw].r) ||
						(picture.get_pixels()[index].r <= 
						picture.get_pixels()[index_se].r))
						processed_picture.get_pixels()[index].r = 0;
					break;
				default:
					break;
			}
			processed_picture.get_pixels()[index].g = processed_picture.get_pixels()[index].r;
			processed_picture.get_pixels()[index].b = processed_picture.get_pixels()[index].r;
		}	
	}

	return processed_picture;
}

// Only keep pixels that are next to at least one strong pixel.
ProcessedPixels Image::hysteresis_filter(const ProcessedPixels& picture) 
{
   const int strong_pixel_threshold = 42;

	ProcessedPixels processed_picture = picture;
	int height = picture.get_height();
	int width = picture.get_width();

	for (int i = 1; i < (height - 1); i++)
		{
		for (int j = 1; j < (width - 1); j++)
		{
			if (processed_picture.get_pixels()[i * width + j].r > strong_pixel_threshold)
			{
				if ((processed_picture.get_pixels()[(i + 0) * width + (j + 1)].r == 0) &&
				   	(processed_picture.get_pixels()[(i - 0) * width + (j - 1)].r == 0) &&
				   	(processed_picture.get_pixels()[(i + 1) * width + (j + 0)].r == 0) &&
				   	(processed_picture.get_pixels()[(i - 1) * width + (j - 0)].r == 0) &&
				   	(processed_picture.get_pixels()[(i + 1) * width + (j - 1)].r == 0) &&
				   	(processed_picture.get_pixels()[(i + 0) * width + (j + 1)].r == 0) &&
				   	(processed_picture.get_pixels()[(i - 1) * width + (j - 1)].r == 0) &&
				   	(processed_picture.get_pixels()[(i - 1) * width + (j + 1)].r == 0))
				{
					processed_picture.get_pixels()[i * width + j].r = 0;
					processed_picture.get_pixels()[i * width + j].g = 0;
					processed_picture.get_pixels()[i * width + j].b = 0;
				}
			}
			else
			{
				processed_picture.get_pixels()[i * width + j].r = 0;
				processed_picture.get_pixels()[i * width + j].g = 0;
				processed_picture.get_pixels()[i * width + j].b = 0;
			}
		}
	}

	return processed_picture;
}

Picture Image::edge_detection(const Picture& picture)
{
	int height = picture.get_height();
	int width = picture.get_width();

	ProcessedPixels processed_picture = convert_to_grayscale(picture);
	processed_picture = gaussian_blur(processed_picture);

	ProcessedPixels cx(width, height);    // Sobel horizontal
	ProcessedPixels cy(width, height);    // Sobel vertical
	ProcessedPixels phase(width, height); // Sobel phase
	processed_picture = sobel_filter(processed_picture, phase);

	processed_picture = non_maximum_suppressor(processed_picture, phase);
	processed_picture = hysteresis_filter(processed_picture);

	Picture final_picture = picture;
	copy_ppixel_to_picture(final_picture, processed_picture);
	
	return final_picture;
}

void algebra::convolution(int* kernel, int kRows, int kCols, const ProcessedPixels& picture,
						  double scaling_factor, ProcessedPixels& p_pixels)
{
	// find center position of kernel (half of kernel size)
	int kCenterX = kCols / 2;
	int kCenterY = kRows / 2;
	int rows = picture.get_height();
	int cols = picture.get_width();
	double current_pixel = 0;

	for(int i=0; i < rows; ++i)              // rows
	{
		for(int j=0; j < cols; ++j)          // columns
	    {
	        for(int m=0; m < kRows; ++m)     // kernel rows
	        {
	            int mm = kRows - 1 - m;      // row index of flipped kernel

	            for(int n=0; n < kCols; ++n) // kernel columns
	            {
	                int nn = kCols - 1 - n;  // column index of flipped kernel

	                // index of input signal, used for checking boundary
	                int ii = i + (m - kCenterY);
	                int jj = j + (n - kCenterX);

	                // ignore input samples which are out of bound
	                if(ii >= 0 && ii < rows && jj >= 0 && jj < cols)
					{
						current_pixel += picture.get_pixels()[ii*cols+jj].r * kernel[mm*kCols+nn];
					}
    	        }				
    	    }

			p_pixels.get_pixels()[i*cols+j].r = current_pixel / scaling_factor;
			p_pixels.get_pixels()[i*cols+j].g = current_pixel / scaling_factor;
			p_pixels.get_pixels()[i*cols+j].b = current_pixel / scaling_factor;
			current_pixel = 0;
   	 	}
	}
}

