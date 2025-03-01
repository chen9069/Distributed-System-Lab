#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client_cache.h"

#include "yfs_lock.h"

class yfs_client {
  extent_client *ec;
	lock_client_cache *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
	int createfile(inum, bool, const char *,inum &, int);
	int lookupfile(const std::string &, const char *, inum &);
	int lookupfile(inum, const char *, inum &);
	int getcontent(inum, std::string &);	
	int get(inum, std::string &);
	int put(inum, std::string);
	int remove(inum);
	int read(inum, size_t, off_t, std::string &);
	int write(inum, size_t, off_t, const char *);
	int setsize(inum, size_t);
	int unlink(inum, const char *);
};

#endif 
