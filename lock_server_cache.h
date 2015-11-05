#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include <vector>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache : public lock_server{
//friend void *retry_t(void *);
 private:
  int nacquire;;
  std::map<lock_protocol::lockid_t, std::vector<std::string> > lock_client_list;
	std::map<std::string, rpcc*> connection;
 public:
  lock_server_cache();
	~lock_server_cache();
	//void retry(lock_protocol::lockid_t);
  lock_protocol::status stat(int clt, lock_protocol::lockid_t, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t, std::string id, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t, std::string id, int &);
};

#endif
