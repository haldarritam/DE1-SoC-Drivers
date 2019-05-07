/* Header for the image class */

#ifndef __PICTURE
#define __PICTURE

#include <string>
#include "Pixel.h"
#include "Shared_Ptr.h"


struct pictureLoadException : public std::exception 
{
	const char * what () const throw () 
	{
		return "Failed to read BMP\n";
	}
};

class Picture
{
public:
	static const int header_size = 54;

	Picture(char* filename);
	Picture(Shared_ptr<Pixel> data, Shared_ptr<byte> header, int width, int height, std::string file_name);
	Picture(const Picture &other); 
	~Picture();

	Picture& operator= (const Picture& other);

	int get_width(void) const {return width;}
	int get_height(void) const {return height;}
	Pixel* get_pixels(void) const {return data.release_ptr();}
	Shared_ptr<byte> get_header(void) const {return header;}
	std::string get_file_name(void) const {return file_name;}

private:
	static const int width_head_pos = 18;
	static const int height_head_pos = 22;

	Shared_ptr<Pixel> data;
	Shared_ptr<byte> header;
	int width;
	int height;
	std::string file_name;

	int read_bmp(char* filename);
	void do_copy(Picture& current, const Picture &other);
};

#endif //__PICTURE
