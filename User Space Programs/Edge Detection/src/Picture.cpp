#include <stdio.h>
#include <stdlib.h>
#include "Picture.h"

Picture::Picture(char* filename)
	: file_name(filename)
{
   // Open input image file (24-bit bitmap image)
   if (read_bmp (filename) < 0)
      throw pictureLoadException();
}

Picture::Picture(Shared_ptr<Pixel> data, Shared_ptr<byte> header, int width, int height, std::string file_name)
	: data(data)
	, header(header)
	, width(width)
	, height(height)
	, file_name(file_name)
{}

Picture::Picture(const Picture &other)
{
	do_copy(*this, other);
}

Picture::~Picture()
{
	// Nothing to do. Internal shared pointers
	// will take care of freeing their own memory.
}

// Read BMP file and extract the header (store in header) and pixel values (store in data)
int Picture::read_bmp(char* filename) 
{
   Pixel * data_;			// temporary pointer to pixel data
   byte * header_;			// temporary pointer to header data
   int width_, height_;		// temporary variables for width and height

   FILE* file = fopen (filename, "rb");
   if (!file) return -1;
   
   // read the 54-byte header
   header_ = new byte[header_size];
   fread (header_, sizeof(byte), header_size, file); 

   // get height and width of image
   width_ = *reinterpret_cast<int*>(&header_[width_head_pos]);	    // width is given by four bytes starting at offset 18
   height_ = *reinterpret_cast<int*>(&header_[height_head_pos]);	// height is given by four bytes starting at offset 22

   // Read in the image
   int size = width_ * height_;
   data_= new Pixel[size];//static_cast<Pixel*>(malloc (size * sizeof(Pixel))); 
   fread (data_, sizeof(Pixel), size, file); // read the rest of the data
   fclose (file);

   header.realloc(header_size);
   data.realloc(size);
   header = header_;	// return pointer to caller
   data = data_;		// return pointer to caller
   width = width_;		// return value to caller
   height = height_;	// return value to caller
   
   return 0;
}

Picture& Picture::operator= (const Picture& other)
{
	do_copy(*this, other);

    return *this;
}

void Picture::do_copy(Picture& current, const Picture &other)
{
	current.data = other.data;
	current.header = other.header;
	current.width = other.width;
	current.height = other.height;
	current.file_name = other.file_name;
}
