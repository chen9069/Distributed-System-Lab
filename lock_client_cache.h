// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "extent_client.h"
#include "lang/verify.h"
#include <set>

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
extent_client* ec;
public:
lock_release_user_cache(rpcc *_cl, std::string _id, extent_client *_ec) : cl(_cl), id(_id), ec(_ec) {}
void dorelease(lock_protocol::lockid_t lid){
//ec->flush(lid);
//lock_protocol::status r;
//lock_protocol::status ret = cl->call(lock_protocol::release, cl->id(), lid, id, r);
//VERIFY (ret == lock_protocol::OK);
}
};

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;
  enum lock_status { NONE, FREE, LOCKED, ACQUIRING, RELEASING, RETRY };
  std::map<lock_protocol::lockid_t, int> lock_status_list;
  std::map<lock_protocol::lockid_t, pthread_cond_t> lock_inUse_cond_list;
  std::map<lock_protocol::lockid_t, pthread_cond_t> lock_retry_cond_list;
  //std::set<long> thread_pool;
 public:
  lock_client_cache(std::string xdst, class extent_client *ec = 0, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);
};


#endif
