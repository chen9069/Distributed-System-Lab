// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
  pthread_mutex_init(&mutex, NULL);
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
// TODO use cond
pthread_mutex_lock(&mutex);
  extent_protocol::status ret = extent_protocol::OK;
	if (attributes.count(eid) == 0) {
pthread_mutex_unlock(&mutex);
		extent_protocol::attr attr;
		ret = cl->call(extent_protocol::getattr, eid, attr);
pthread_mutex_lock(&mutex);
		attributes[eid] = attr;
	}
	attributes[eid].atime = time(NULL);

	if (contents.count(eid) == 0) {
pthread_mutex_unlock(&mutex);
  	ret = cl->call(extent_protocol::get, eid, buf);
pthread_mutex_lock(&mutex);
		contents[eid] = buf;
}
	buf = contents[eid];
if (ret != extent_protocol::OK) {
	attributes.erase(eid);
	contents.erase(eid);
}
pthread_mutex_unlock(&mutex);
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
pthread_mutex_lock(&mutex);
  extent_protocol::status ret = extent_protocol::OK;
	if (attributes.count(eid) == 0) {
pthread_mutex_unlock(&mutex);
  	ret = cl->call(extent_protocol::getattr, eid, attr);
pthread_mutex_lock(&mutex);
		attributes[eid] = attr;
}
	attr = attributes[eid];
if (ret != extent_protocol::OK) {
	attributes.erase(eid);
	contents.erase(eid);
}
pthread_mutex_unlock(&mutex);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
pthread_mutex_lock(&mutex);
  extent_protocol::status ret = extent_protocol::OK;
	attributes[eid].mtime = time(NULL);
	attributes[eid].ctime = time(NULL);
	attributes[eid].size = buf.length();
	contents[eid] = buf;
  //int r;
  //ret = cl->call(extent_protocol::put, eid, buf, r);
pthread_mutex_unlock(&mutex);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
pthread_mutex_lock(&mutex);
  extent_protocol::status ret = extent_protocol::OK;
	attributes.erase(eid);
	contents.erase(eid);
  //int r;
  //ret = cl->call(extent_protocol::remove, eid, r);
pthread_mutex_unlock(&mutex);
  return ret;
}

extent_protocol::status 
extent_client::flush(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
pthread_mutex_lock(&mutex);
	if (attributes.count(eid) == 0) {
printf("REMOVE!\n");
pthread_mutex_unlock(&mutex);
  	int r;
		ret = cl->call(extent_protocol::remove, eid, r);
pthread_mutex_lock(&mutex);
	}
	else {
printf("FLUSH!\n");
		if (contents.count(eid) != 0) {
			std::string buf = contents[eid];
pthread_mutex_unlock(&mutex);
  		int r;
  		ret = cl->call(extent_protocol::put, eid, buf, r);
pthread_mutex_lock(&mutex);
		}
	}

	attributes.erase(eid);
	contents.erase(eid);
pthread_mutex_unlock(&mutex);
	return ret;
}


