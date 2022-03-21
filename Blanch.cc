/*-
 * Copyright (c) 2015-2022
 *  Robert White <rwhite@pobox.com>
 *  License: GPL 3.0 or higher
 */

#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <string>

#include <boost/program_options.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifndef O_LARGEFILE
/* FreeBSD doesn't have this definition */
#define O_LARGEFILE (0)
#endif

/*
 * Copyright © (c) Robert White <rwhite@pobox.com> 2011
 * Licensed for distribution under the GPL v3
 *
 * see http://www.gnu.org/licenses/gpl.html
 */

/*
 * Blanch, french for "to make white", a cooking technique
 *   to destroy unwanted enzimes in vegabables or prepare
 *   meat prior to freezing etc.
 *
 * This program is used to flood a storage medium, such
 *   as a raw disk or a file system, with a single character,
 *   by default 0x00, repeated as many times as will fit.
 *   Alternately a randomized buffer is repeatedly written.
 *   Alternately a repeatedly randomized buffer is written.
 *
 * The program is optimized for linux and direct IO. This
 *   is much faster than dd(1) because it doesn't read
 *   from anywhere (e.g. /dev/zero) nor does it need to
 *   copy any data from user to kernel space, and finally
 *   it doesn't involve the disk buffer system if that
 *   can be avoided by the kernel. This can save four or
 *   more memcpy() type operations for the entire size of
 *   the eventual flood.
 *
 * Usage Blanch [file_name [(flood_character | "r" | "rr" )]]
 *     file_name defaults to "scratch.null", there is
 *       no magic to this name, so any legal file name
 *       will do including a device. Using "" will use
 *       the default (good for just changing the
 *       flood_character).
 *     flood_character is the integer number for the
 *       character to use in any C/C++ format, e.g.
 *       0, 255 == 0xff == 0377, only the lowest 8
 *       bits are significant.
 *     "r" (randomize) Flood the buffer with lrand48()
 *       integers once before writing commences.
 *     "rr" (repeatedly randomize) Flood the buffer with
 *       fresh lrand48() integers before each write.
 */

namespace po = boost::program_options;

using namespace std;

inline ssize_t
xfer(bool reading, int fd, void *buf, size_t count)
{
  return reading ? read(fd,buf,count) : write(fd,buf,count);
}

int main(int argc, char * argv[])
{
  const size_t	expanse=1024*1024*2;
  string	filename;
  off64_t	skip=0;
  int		number = 0;
  long long	bytes_total = 0;
  ssize_t	bytes_written = 0;
  int		counter = 0;
  bool		direct = true;
  int		random = 0;
  int		fill = 0;
  bool		reading = false;

  cin.unsetf(ios::hex | ios::dec | ios::oct );

  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "this help message")
    ("filename,f", po::value<string>(&filename)->default_value("scratch.null"),"file or device name")
    ("skip,s", po::value<off64_t>(&skip)->default_value(0), "skip (bytes) before writing")
    ("number,n", po::value<int>(&number)->default_value(0), "number of 2MB blocks to write")
    ("direct", po::value<bool>(&direct)->default_value(true), "use direct io")
    ("reading", po::value<bool>(&reading)->default_value(false), "read file instead of write")
    ("random,r",po::value(&random)->default_value(0), "randomize contents of file")
    ;

  po::variables_map vm;
  po::store(parse_command_line(argc, argv, desc),vm);
  po::notify(vm);
  if (vm.count("help")) {
    cout << desc << endl;
    return 0;
  }

  void *	buffer = mmap(0,expanse,
				PROT_READ|PROT_WRITE,
				MAP_SHARED|MAP_ANONYMOUS,
				-1,0);
  int		fd=open(filename.c_str(),
  			O_RDWR|O_CREAT|O_DIRECT|O_LARGEFILE|O_NOFOLLOW,
			S_IRUSR|S_IWUSR);
  if (fd < 0 && errno == EINVAL) {
    cerr << "Could Not Open File \"" << filename << "\":" << strerror(errno) << ":Trying Again without DIRECT_IO" << endl;
    fd=open(filename.c_str(),
  	O_RDWR|O_CREAT|O_LARGEFILE|O_NOFOLLOW,
	S_IRUSR|S_IWUSR);
    if (fd >= 0) {
      // do what we can to mitigate the direct IO failure.
      posix_fadvise(fd,0,0,POSIX_FADV_NOREUSE);
      direct && fcntl(fd,F_SETFL,static_cast<long>(O_DIRECT)); // Try the back door
    }
  }
  if (fd < 0) {
    cerr << "Could Not Open File \"" << filename << "\":" << strerror(errno) << endl;
    exit(1);
  }

  int * const	base = static_cast<int *>(buffer);
  int * const	extent = (base + (expanse / sizeof(int)));
  if (!reading) {
    if (random) {
      long		seed;
      fstream entropy("/dev/urandom",ios::in|ios::binary);
      entropy.read(reinterpret_cast<char *>(&seed),sizeof(seed));
      srand48(seed);
      generate(base,extent,lrand48);
      cerr << "Buffer Pointer: " << buffer
	   << " Fill " << ((random>1)?"Regenerating":"Repeating")
	   << " Random Buffer, Write Size: " << dec << (expanse>>10) << "kb"
	   << endl;
    } else {
      cerr << "Buffer Pointer: " << buffer
	   << " Fill Character: 0x" << hex << (fill&0xFF)
	   << " Write Size: " << dec << (expanse>>9) << "kb"
	   << endl;
      memset(buffer,(fill&0xFF),expanse);
    }
  }
  if (skip) {
     int64_t	sought = lseek(fd,skip,SEEK_SET);
     cerr << "Offset before transfer is " << sought << endl;
  }
  cerr << "Transfer Commenses: " << endl;
  while ((0 < (bytes_written = xfer(reading,fd,buffer,expanse))) && (++counter != number)) {
    bytes_total += bytes_written;
    if (0 == counter % 10) cerr << "Xfer #: " << setw(5) << counter
				<< " Bytes: " << setw(10) << bytes_written
    				<< " Total: " << (bytes_total >> 9)
				<< "kb\r"
				<< flush;
      if (random > 1) generate(base,extent,lrand48);
  }
  fsync(fd);
    if (bytes_written > 0) bytes_total += bytes_written;
    cerr << "Xfer #: " << setw(5) << counter
    	<< " Bytes: " << setw(10) << bytes_written
    	<< " Total: " << (bytes_total >> 9) << "kb"
	<< endl;
  close(fd);
  return 0;
}

