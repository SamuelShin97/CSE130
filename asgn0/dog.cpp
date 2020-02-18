#include <iostream>
#include<sys/types.h>
#include<sys/stat.h>
#include <fcntl.h> 
#include <string>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <vector>

using namespace std;

void echo_user_input()
{
   while(!cin.eof())
      {
         string input;
         getline(cin, input);
	 if (!cin.eof())
	 {
            cout << input << endl;
	 }
      }
   cin.clear();
}

bool check_for_warning(int indicator, char* filename)
{
   if (indicator == -1)
   {
      warn("%s", filename);
      return true;
   }
   return false;
}



void process_file(char* file)
{
   size_t cnt = 32000;
   char file_contents[cnt];

   int opened = open(file, O_RDONLY);
   bool warning_given = check_for_warning(opened, file);

   size_t bytes_read_sentinel= 1; //initialize to a non-zero value
   while (bytes_read_sentinel != 0)
   {
      size_t bytes_read = read(opened, &file_contents, cnt);
      if (!warning_given)
      {
         warning_given = check_for_warning(bytes_read, file);
      }
      else
      {
         break;
      }

      size_t bytes_written = write(STDOUT_FILENO, &file_contents, bytes_read);
      if (!warning_given)
      {
         warning_given = check_for_warning(bytes_written, file);
      }
      else
      {
         break;
      }

      bytes_read_sentinel = bytes_read;

      if (bytes_read_sentinel == 0)
      {
         break;
      }
   }

   int closed = close(opened);
   if (!warning_given)
   {
      check_for_warning(closed, file);
   }

}

int main(int argc, char** argv)
{
   if (argc > 1)
   {
      for (int i = 1; i < argc; i++)
      {
	 if (strcmp(argv[i], "-") == 0)
	 {
	    echo_user_input();
         }
	 else
	 {
	    process_file(argv[i]);
	 }
      }
   }

   else
   {
      echo_user_input();
   }

   return 0;
}
