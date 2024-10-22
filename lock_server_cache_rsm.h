#ifndef lock_server_cache_rsm_h
#define lock_server_cache_rsm_h

#include <string>

#include "lock_protocol.h"
#include "rpc.h"
#include "rsm_state_transfer.h"
#include "rsm.h"

class lock_server_cache_rsm : public rsm_state_transfer {
 private:
  int nacquire;
  pthread_mutex_t mutex;
	pthread_cond_t revoke_cond;
	pthread_cond_t retry_cond;
	std::map<lock_protocol::lockid_t, bool> locks;
  std::map<lock_protocol::lockid_t, std::vector<std::string> > lock_client_list;
	std::map<int, std::map<lock_protocol::lockid_t, lock_protocol::xid_t> > client_lock_id;
	//std::map<std::string, rpcc*> connection;
  class rsm *rsm;
 public:
  lock_server_cache_rsm(class rsm *rsm = 0);
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  void revoker();
  void retryer();
  std::string marshal_state();
  void unmarshal_state(std::string state);
  int acquire(int clt, lock_protocol::lockid_t, std::string id, 
	      lock_protocol::xid_t, int &);
  int release(int clt, lock_protocol::lockid_t, std::string id, lock_protocol::xid_t,
	      int &);
};

#endif
