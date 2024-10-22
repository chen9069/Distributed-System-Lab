// lock client interface.

#ifndef lock_client_cache_rsm_h

#define lock_client_cache_rsm_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"

#include "rsm_client.h"

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_release_user_cache : public lock_release_user {
rpcc *cl;
std::string id;
public:
lock_release_user_cache(rpcc *_cl, std::string _id) : cl(_cl), id(_id) {}
void dorelease(lock_protocol::lockid_t lid){
//ec->flush(lid);
//lock_protocol::status r;
//lock_protocol::status ret = cl->call(lock_protocol::release, cl->id(), lid, id, r);
//VERIFY (ret == lock_protocol::OK);
}
};

class lock_client_cache_rsm;

// Clients that caches locks.  The server can revoke locks using 
// lock_revoke_server.
class lock_client_cache_rsm : public lock_client {
 private:
  rsm_client *rsmc;
  //class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  lock_protocol::xid_t xid;
  enum lock_status { NONE, FREE, LOCKED, ACQUIRING, RELEASING, RETRY };
  std::map<lock_protocol::lockid_t, int> lock_status_list;
  std::map<lock_protocol::lockid_t, pthread_cond_t> lock_inUse_cond_list;
  std::map<lock_protocol::lockid_t, pthread_cond_t> lock_retry_cond_list;
	pthread_cond_t revoke_cond;
 public:
  static int last_port;
  lock_client_cache_rsm(std::string xdst);
  virtual ~lock_client_cache_rsm() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  virtual lock_protocol::status release(lock_protocol::lockid_t);
  void releaser();
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, int &);
};


#endif
