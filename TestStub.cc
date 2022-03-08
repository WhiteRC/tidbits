#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <set>
#include <linux/kdev_t.h>

#include "FileEntries.h"


using namespace FileEntries;

using namespace std;
typedef list<string> NameList;
typedef list<string> NameSet;

int
main (int argc, char *argv[])
{
  int counter;
  NameList names;
  {
    NameSet DeviceFS;
    NameSet NoDeviceFS;
    NameSet::iterator cursor;
    NameSet::iterator extent;
    ifstream filesystems("/proc/filesystems");
    string scratch;
    for (filesystems >> scratch; filesystems; filesystems >> scratch) {
      if (scratch == "nodev") {
        filesystems >> scratch;
        NoDeviceFS.push_back(scratch);
      } else {
        DeviceFS.push_back(scratch);
      }
    }
    cout << "Device FileSystems:" << endl;
    extent = DeviceFS.end();
    for (cursor = DeviceFS.begin(); cursor != extent; ++cursor) {
      cout << "  " << *cursor << endl;
    }
    cout << "Non-Device FileSystems:" << endl;
    extent = NoDeviceFS.end();
    for (cursor = NoDeviceFS.begin(); cursor != extent; ++cursor) {
      cout << "  " << *cursor << endl;
    }
  }
  FileSet listing;
  FileSet::index<FileEntry::NameIndex>::type & listing_view =
    listing.get<FileEntry::NameIndex>();
  for (counter=1; counter < argc; ++counter) {
    names.push_back(std::string(argv[counter]));
  }
  {
    NameList::iterator cursor = names.begin();
    NameList::iterator extent = names.end();
    for (; cursor != extent; ++cursor) {
      FileWrapper	file(cursor->c_str(),O_DIRECTORY|O_RDONLY);
      ::FileEntries::Fill(file,listing,shallow_selector());
    }
  }
  {
    FileWrapper blockdevs("/sys/block",O_DIRECTORY|O_RDONLY);
    Fill(blockdevs,listing,deep_selector("[hsv][dr][a-z]?[0-9]?","[hsv][dr][a-z]?[0-9]?"));
  }
  {
    FileSet::index<FileEntry::NameIndex>::type::iterator cursor = listing_view.begin();
    FileSet::index<FileEntry::NameIndex>::type::iterator extent = listing_view.end();
    for (; cursor != extent; ++cursor) {
      std::cout
        << std::hex
        << cursor->Device
        << std::dec
        << ":"
        << MAJOR(cursor->Device)
	<< ":"
	<< MINOR(cursor->Device)
	<< ":"
	<< cursor->ParentINode
	<< ":"
        << cursor->INode
        << ":"
	<< cursor->Name
	<< ":"
	<< cursor->Type
	<< std::endl;
    }
  }
  return 0;
}
