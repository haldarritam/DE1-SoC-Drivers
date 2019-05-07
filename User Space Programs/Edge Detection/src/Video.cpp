/* Definitions for video interface class */
#include <stdio.h>
#include <stdlib.h>
#include "Video.h"
#include "Picture.h"
//#include <intelfpgaup/video.h>

using namespace Video;

VGA::VGA()
{
   if (!video_open())
      throw OpenVideoException();

   video_read(&screen_x, &screen_y, &char_x, &char_y); 
}

VGA::~VGA()
{
	video_close();
}

// Draw the image pixels on the VGA display
void VGA::draw_image(const Picture& picture) 
{
	const int shift_msb_rb = 3;
	const int shift_msb_g  = 2;
	const int shift_565_g  = 5;
	const int shift_565_r  = 11;
	int centralizer_offset = 0;

	int t_col = picture.get_width();
	int t_row = picture.get_height();

	if (t_col < screen_x)
		centralizer_offset = (screen_x - t_col)/2;

	video_clear();
	for(int col = 0; col < t_col && col < screen_x; col++)
		for (int row = 0; row < t_row && row < screen_y; row++)
		{
			 video_pixel(col + centralizer_offset, row, 
						(picture.get_pixels()[row*t_col+col].b >> shift_msb_rb) |
						(picture.get_pixels()[row*t_col+col].g >> shift_msb_g)  << shift_565_g |
						(picture.get_pixels()[row*t_col+col].r >> shift_msb_rb) << shift_565_r);
		}

	video_show();
}
