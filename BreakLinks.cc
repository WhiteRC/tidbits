#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <string>
#include <iostream>

/*
 * Copyright © (c) Robert White <rwhite@pobox.com> 2005-2011
 * Licensed for distribution under the GPL v3
 *
 * see http://www.gnu.org/licenses/gpl.html
 */

template <class T>
T min(const T left, const T right) {
	return (left < right) ? left : right;
}

#define MAX_BLOCK	(1024*8)

unsigned char NullBuffer[MAX_BLOCK] = {}; // all zeros for memcmp() for sparse fill
unsigned char DataBuffer[MAX_BLOCK] = {}; 

int main(int argc, char * argv[])
{
	int retval = 0;
	struct stat	TargetStat;
	struct stat	LinkStat;
	for (int counter = 1; counter < argc; ++counter)	{
		
		if (stat(argv[counter],&TargetStat))	{
			std::string	scratch;
			scratch += "Couldn't stat(2) File \"";
			scratch += argv[counter];
			scratch += '"';
			perror(scratch.c_str());
			continue;
		}
		if (lstat(argv[counter],&LinkStat))	{
			std::string	scratch;
			scratch += "Couldn't lstat(2) File \"";
			scratch += argv[counter];
			scratch += '"';
			perror(scratch.c_str());
			continue;
		}
		if ((TargetStat.st_nlink == 1) && (!S_ISLNK(LinkStat.st_mode)))	{
			// nothing worth doing...
			std::cerr << "File \""
				  << argv[counter]
				  << "\" Skipped: File Not Multiply Linked."
				  << std::endl;
			continue;
		}
		if (!S_ISREG(TargetStat.st_mode))	{
			// nothing worth doing...
			std::cerr << "File \""
				  << argv[counter]
				  << "\" Skipped: Object Is Not Regular File."
				  << std::endl;
			continue;
		}
		std::string	FileName(argv[counter]);
		std::string	OldName(FileName + ".BREAKLINK");
		std::string	NewName(FileName + ".UNLINKED");
		int oldfd = open(FileName.c_str(),O_RDONLY);
		if (oldfd < 0)	{
			std::string	scratch;
			scratch += "Couldn't open(2) File \"";
			scratch += FileName;
			scratch += '"';
			perror(scratch.c_str());
			continue;
		}
		int newfd = creat(NewName.c_str(),(TargetStat.st_mode & ~S_IFMT));
		if (newfd < 0)	{
			std::string	scratch;
			scratch += "Couldn't creat(2) File \"";
			scratch += NewName;
			scratch += '"';
			perror(scratch.c_str());
			close(oldfd);
			continue;
		}
		bool	CopiedOK = true;
		off_t	BytesCopied = 0;
		ssize_t	TargetReadSize(min<ssize_t>(TargetStat.st_blksize,MAX_BLOCK));
		ftruncate(newfd,TargetStat.st_size);
		while ((CopiedOK) && (BytesCopied < TargetStat.st_size))	{
			const ssize_t	ReadSize(BytesCopied%TargetReadSize ? BytesCopied%TargetReadSize : TargetReadSize);
			ssize_t		BytesRead = read(oldfd,DataBuffer,ReadSize);
			if (BytesRead < 0)	{
				std::string	scratch;
				scratch += "Couldn't read(2) File \"";
				scratch += FileName;
				scratch += '"';
				perror(scratch.c_str());
				CopiedOK = false;
			}
			if (BytesRead == 0)	{
				std::string	scratch;
				scratch += "Earily EOF in read(2) of File \"";
				scratch += FileName;
				scratch += ": Copy Aborted: File Mutating?";
				std::cerr << scratch << std::endl;
				CopiedOK = false;
			}
			if (memcmp(NullBuffer,DataBuffer,BytesRead) != 0) {
				ssize_t BytesWritten = write(newfd,DataBuffer,BytesRead);
				if (BytesWritten < 0)	{
					std::string	scratch;
					scratch += "Couldn't write(2) File \"";
					scratch += NewName;
					scratch += '"';
					perror(scratch.c_str());
					CopiedOK = false;
				}
				if (BytesWritten < BytesRead)	{
					std::string	scratch;
					scratch += "Unexpected Partial write(2) of File \"";
					scratch += NewName;
					scratch += ": Copy Aborted: Out Of Space?";
					std::cerr << scratch << std::endl;
					CopiedOK = false;
				}
				BytesCopied += BytesWritten;
			} else {
				// For SPARSE files we just skip over the zeros as if we wrote them
				BytesCopied += BytesRead;
				if (lseek(newfd,BytesRead,SEEK_CUR) != BytesCopied) {
					std::string	scratch;
					scratch += "Error in lseek(2) of File \"";
					scratch += NewName;
					scratch += ": Copy Aborted: Out Of Space?";
					std::cerr << scratch << std::endl;
					CopiedOK = false;
				}
			}
		}
		fsync(newfd);
		close(newfd);
		close(oldfd);
		if (chown(NewName.c_str(),TargetStat.st_uid,TargetStat.st_gid))	{
			std::string	scratch;
			scratch += "Couldn't chown(2) File \"";
			scratch += NewName;
			scratch += '"';
			perror(scratch.c_str());
			CopiedOK = false;
		}
		if (chmod(NewName.c_str(),(TargetStat.st_mode & (~S_IFMT))))	{
			std::string	scratch;
			scratch += "Couldn't chmod(2) File \"";
			scratch += NewName;
			scratch += '"';
			perror(scratch.c_str());
			CopiedOK = false;
		}
		struct utimbuf Times = {0};
		Times.actime = TargetStat.st_atime;
		Times.modtime = TargetStat.st_mtime;
		if (utime(NewName.c_str(),&Times))	{
			std::string	scratch;
			scratch += "Couldn't utime(2) File \"";
			scratch += NewName;
			scratch += '"';
			perror(scratch.c_str());
			CopiedOK = false;
		}
		if (CopiedOK)	{
			if (rename(FileName.c_str(),OldName.c_str()))	{
				std::string	scratch;
				scratch += "Couldn't rename(2) File \"";
				scratch += FileName;
				scratch += "\" to \"";
				scratch += OldName;
				scratch += '"';
				perror(scratch.c_str());
				unlink(NewName.c_str());
			} else	{
				if (rename(NewName.c_str(),FileName.c_str()))	{
					std::string	scratch;
					scratch += "Couldn't rename(2) File \"";
					scratch += NewName;
					scratch += "\" to \"";
					scratch += FileName;
					scratch += '"';
					perror(scratch.c_str());
					unlink(NewName.c_str());
					if (rename(OldName.c_str(),FileName.c_str()))	{
						std::string	scratch;
						scratch += "Couldn't rename(2) File \"";
						scratch += OldName;
						scratch += "\" back to \"";
						scratch += FileName;
						scratch += "\": Original File Now Unavailable!";
						perror(scratch.c_str());
						unlink(NewName.c_str());
					}
				} else	{
					unlink(OldName.c_str());
				}
			}
		} else	{
			unlink(NewName.c_str());
		}
	}
	return retval;
}
