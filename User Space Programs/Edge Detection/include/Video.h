/* Header for video interface class */

#ifndef __VIDEO
#define __VIDEO

#include <exception>
#include "Pixel.h"
#include "Picture.h"

// Opens the VGA device driver
extern "C" int video_open(void);
// Closes the video device
extern "C" void video_close(void);
// Reads cols, rows, txt_cols, txt_rows
extern "C" int video_read(int*, int*, int*, int*);
// Clears all graphics from the video display
extern "C" void video_clear(void);
// Swaps the video front and back buffers
extern "C" void video_show(void);
// Sets pixel at (x, y) to color
extern "C" void video_pixel(int, int, short);

namespace Video
{
	struct OpenVideoException : public std::exception 
	{
   		const char * what () const throw () 
		{
			return "Could not open video device\n";
   		}
	};

	class VGA
	{
	public:
		VGA();
		~VGA();
		void draw_image(const Picture&);

	private:
		int screen_x;
		int screen_y;
		int char_x;
		int char_y;
	};
}


#endif //__VIDEO
