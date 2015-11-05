// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client_cache.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
	lc = new lock_client_cache(lock_dst, ec);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
	YFSLock yl(inum, lc, ec);
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock

  //printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  //printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
	YFSLock yl(inum, lc, ec);
  int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}

int yfs_client::createfile(inum dinum, bool f, const char* name, inum& inum, int pid) {
	YFSLock yl1(dinum, lc, ec);
	// file name doesn't allow spaces!!
	extent_protocol::attr a;
	//srandom(getpid());
	inum = random();
	if (f)
  	inum |= 0x80000000;
	else
		inum &= 0x7FFFFFFF;
	YFSLock yl2(inum, lc, ec);
	//printf("inum: %016llx\n", inum);
	//lc->acquire(inum);
/*
	while (ec->getattr(inum, a) == extent_protocol::OK) {
		lc->release(inum);
		printf("finding next!\n");
		inum ++;
		if (f)
  		inum |= 0x80000000;
		else
			inum &= 0x7FFFFFFF;
		lc->acquire(inum);
	}
*/
	//printf("file inum: %016llx\n", inum);

	std::string dcontent;
	if (get(dinum, dcontent) != OK) {
		//lc->release(inum);
		return NOENT;
	}
	// printf("dir content: %s\n", dcontent.c_str());
	yfs_client::inum finum;
	if (lookupfile(dcontent, name, finum) != OK) {
		//lc->release(inum);
		return EXIST;
	}
	std::ostringstream ost;
	ost << dcontent << " " << inum << '@' << name;
	if (put(dinum, ost.str()) != OK || put(inum, "") != OK) {
		//lc->release(inum);
		return IOERR;
	}

	//lc->release(inum);
	return OK;
}


int yfs_client::lookupfile(const std::string& dcontent, const char* name, inum& inum) {
	std::istringstream ist(dcontent);
	std::string file;
	while (ist >> file) {
		std::istringstream iss(file);
		unsigned long long finum;
		std::string fname;
		iss >> finum;
		iss.get();
		iss >> fname;
		// printf("finum: %016llx, fname: %s\n", finum, fname.c_str());
		if (strcmp(fname.c_str(), name) == 0) {
			inum = finum;
			return EXIST;
		}
	}
	return OK;
}

int yfs_client::lookupfile(inum dinum, const char* name, inum& inum) {
	YFSLock yl1(dinum, lc, ec);
	std::string dcontent;
	if (get(dinum, dcontent) != OK) {
		return NOENT;
	}
	printf("content: %s\n", dcontent.c_str());
	return lookupfile(dcontent, name, inum);
}


int yfs_client::getcontent(inum inum, std::string& buf) {
	YFSLock yl(inum, lc, ec);
	return get(inum, buf);
}


int yfs_client::put(inum inum, std::string content) {
	if (ec->put(inum, content) != extent_protocol::OK) {
		printf("cannot put: %016llx\n", inum);
		printf("buf: %s\n", content.c_str());
		return IOERR;
	}
	return OK;
}

int yfs_client::get(inum inum, std::string& buf) {
	if (ec->get(inum, buf) != extent_protocol::OK) {
		printf("cannot read: %016llx\n", inum);
		printf("buf: %s\n", buf.c_str());
		return NOENT;
	}
	return OK;
}

int yfs_client::remove(inum inum) {
	YFSLock yl(inum, lc, ec);
	if (ec->remove(inum) != extent_protocol::OK) {
		printf("cannot remove: %016llx\n", inum);
		return NOENT;
	}
	return OK;
}

int yfs_client::setsize(inum inum, size_t size) {
	YFSLock yl(inum, lc, ec);
	std::string buf;
	if (get(inum, buf) != OK) {
		return NOENT;
	}
	if (buf.length() > size) {
		buf = buf.substr(0, size);
	}
	else {
		for (int i = 0; i < size - buf.length(); i ++) {
			buf.append(" ");
		}
	}
	if (put(inum, buf) != OK) {
		return IOERR;
	}
	return OK;
}

int yfs_client::read(inum inum, size_t size, off_t off, std::string& buf) {
	YFSLock yl(inum, lc, ec);
	if (get(inum, buf) != OK) {
		return NOENT;
	}
	off = off > buf.length() ? buf.length() : off;
	buf = buf.substr(off, size);
	return OK;
}

int yfs_client::write(inum inum, size_t size, off_t off, const char* buf) {
	YFSLock yl(inum, lc, ec);
	std::string content;
	//printf ("write: %lld\n", inum);
	if (get(inum, content) != OK) {
		return NOENT;
	} 
	std::string end;	
	if (off + size < content.length()) {
		end = content.substr(off + size);
	}
	if (off <= content.length()) {
		content = content.substr(0, off);
	}
	else {
		int n = content.length();
		for (int i = 0; i < off - n; i ++)
			content.push_back('\0');
	}
	content.append(buf, size);
	content.append(end);
	if (put(inum, content) != OK) {
		return IOERR;
	}
	return OK;
}

int yfs_client::unlink(inum parent, const char* name) {
	YFSLock yl(parent, lc, ec);
	if (!isdir(parent)) {
		return NOENT;
	}
	std::string content;
	if (get(parent, content) != OK) {
		return NOENT;
	} 
	std::istringstream ist(content);
	std::string file;
	std::ostringstream ost;
	bool found = false;
	int count = 0;
	inum inum;
	while (ist >> file) {
		std::istringstream iss(file);
		unsigned long long finum;
		std::string fname;
		iss >> finum;
		iss.get();
		iss >> fname;
		// printf("finum: %016llx, fname: %s\n", finum, fname.c_str());
		if (strcmp(fname.c_str(), name) != 0 || isdir(finum)) {
			if (count++ > 0)
				ost << " ";
			ost << file;
		}
		else {
			found = true;
			inum = finum;
		}
	}
	if (!found) {
		return NOENT;
	}
	else {
		if (remove(inum) != OK) {
			return IOERR;
		}
		if (put(parent, ost.str()) != OK) {
			return IOERR;
		}
		return OK;
	}
}

