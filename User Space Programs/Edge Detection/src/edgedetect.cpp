#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <exception>
#include "Image.h"
#include "Video.h"
#include "Picture.h"

using namespace DSP;
using namespace Video;

int main(int argc, char *argv[]) {
   const int success = 0;
   const int fatal_exception = -1;
   const int argc_error = -2;
   const int expected_argc = 2;

   time_t start, end;									// used to measure the program's run-time
   Image imageProcess;									// object to deal with image processing
   
   // Check inputs
   if (argc < expected_argc) 
   {
      printf ("Usage: edgedetect <BMP filename>\n");
      return argc_error;
   }

   try
   {
	  const int file_name_pos = 1;
      Picture picture(argv[file_name_pos]);
	  VGA video_output;

	  picture = imageProcess.flip(picture);
      video_output.draw_image(picture);

      /********************************************
      *          IMAGE PROCESSING STAGES          *
      ********************************************/
   
      // Start measuring time
      start = clock();

	  picture = imageProcess.edge_detection(picture);

	  // Stop measuring time
      end = clock();

	  video_output.draw_image(picture);    
      printf ("TIME ELAPSED: %.0f ms\n", ((double) (end - start)) * 1000 / CLOCKS_PER_SEC);
   
      printf ("Press return to continue");
      getchar();
      //draw_image (data, width, height, screen_x, screen_y);
   }
   catch(std::exception& e) 
   {
      printf("%s", e.what());
      return fatal_exception;
   }
    
   return success;
}
