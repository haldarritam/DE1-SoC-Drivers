/* Definitions for the ProcessedPixels class */

#include "Pixel.h"

ProcessedPixels::ProcessedPixels(int width, int height)
{
	p_pixel = new PPixel[width * height];
	this->width = width;
	this->height = height;
}

ProcessedPixels::ProcessedPixels(const ProcessedPixels &other)
{
	do_copy(*this, other);
}

ProcessedPixels::~ProcessedPixels()
{
	// Nothing to do. Internal shared pointers
	// will take care of freeing their own memory.
}

ProcessedPixels& ProcessedPixels::operator= (const ProcessedPixels& other)
{
	do_copy(*this, other);

	return *this;
}

void ProcessedPixels::do_copy(ProcessedPixels& current, const ProcessedPixels& other)
{
	current.p_pixel = other.p_pixel;
	current.width = other.width;
	current.height = other.height;
}
