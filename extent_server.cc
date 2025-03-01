// the extent server implementation

#include "extent_server.h"
#include "slock.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {
	pthread_mutex_init(&mutex, NULL);
	contents[1] = "";
	extent_protocol::attr a;
	a.mtime = 0;
	a.ctime = 0;
	a.atime = 0;
	a.size = 0;
	attributes[1] = a;
	printf("start\n");
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  //return extent_protocol::IOERR;
	ScopedLock sl(&mutex);
  contents[id] = buf;
	time_t now = time(NULL);
	extent_protocol::attr a;
	a.mtime = now;
	a.ctime = now;
	a.size = buf.length();
	attributes[id] = a;
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
	ScopedLock sl(&mutex);
  if (contents.count(id) == 0) {
  	return extent_protocol::IOERR;
	}
	buf = contents[id];
	attributes[id].atime = time(NULL);
	return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
	ScopedLock sl(&mutex);
	if (attributes.count(id) == 0)
		return extent_protocol::IOERR;
	a = attributes[id];
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
	ScopedLock sl(&mutex);
  if (contents.count(id) == 0)
  	return extent_protocol::IOERR;
	contents.erase(id);
	attributes.erase(id);
	return extent_protocol::OK;
}

