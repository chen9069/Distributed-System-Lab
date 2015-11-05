// the caching lock server implementation

#include "lock_server_cache_rsm.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"
#include "rpc/marshall.h"


static void *
revokethread(void *x)
{
  lock_server_cache_rsm *sc = (lock_server_cache_rsm *) x;
  sc->revoker();
  return 0;
}

static void *
retrythread(void *x)
{
  lock_server_cache_rsm *sc = (lock_server_cache_rsm *) x;
  sc->retryer();
  return 0;
}

lock_server_cache_rsm::lock_server_cache_rsm(class rsm *_rsm) 
  : rsm (_rsm), nacquire(0)
{
  pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&revoke_cond, NULL);
	pthread_cond_init(&retry_cond, NULL);
  pthread_t th;
  int r = pthread_create(&th, NULL, &revokethread, (void *) this);
  VERIFY (r == 0);
  r = pthread_create(&th, NULL, &retrythread, (void *) this);
  VERIFY (r == 0);
}

void
lock_server_cache_rsm::revoker()
{
  // This method should be a continuous loop, that sends revoke
  // messages to lock holders whenever another client wants the
  // same lock
  pthread_mutex_lock(&mutex);
while (true) {
	printf("running revoker\n");
	if (rsm->amiprimary()) {
		std::map<lock_protocol::lockid_t, bool>::iterator iter = locks.begin();		
		for (; iter != locks.end(); iter ++) {
			lock_protocol::lockid_t lid = iter->first;
			if (locks[lid]) {
				std::vector<std::string> vec = lock_client_list[lid];
				if (vec.size() > 1) {
					std::string client_dst = lock_client_list[lid].front();
  				handle h(client_dst);
					rpcc *cl = h.safebind();
					if(!cl){
						printf("decide bind fail\n");
					}
					int rr;
					printf("(primary) sending revoke to %s\n", client_dst.c_str());
					rlock_protocol::status ret = cl->call(rlock_protocol::revoke, lid, rr);
					VERIFY (ret == rlock_protocol::OK);
				}
			}
		}
	}
	pthread_cond_wait(&revoke_cond, &mutex);
}

}


void
lock_server_cache_rsm::retryer()
{

  // This method should be a continuous loop, waiting for locks
  // to be released and then sending retry messages to those who
  // are waiting for it.
pthread_mutex_lock(&mutex);
while (true) {
	printf("running retrier\n");
	if (rsm->amiprimary()) {
		std::map<lock_protocol::lockid_t, bool>::iterator iter = locks.begin();		
		for (; iter != locks.end(); iter ++) {
			lock_protocol::lockid_t lid = iter->first;
			if (!locks[lid]) {
				std::vector<std::string> vec = lock_client_list[lid];
				if (vec.size() > 0) {
					std::string client_dst = lock_client_list[lid].front();
  				handle h(client_dst);
					rpcc *cl = h.safebind();
					if(!cl){
						printf("decide bind fail\n");
					}
					int rr;
					printf("(primary) sending retry to %s\n", client_dst.c_str());
					rlock_protocol::status ret = cl->call(rlock_protocol::retry, lid, rr);
					VERIFY (ret == rlock_protocol::OK);
				}
			}
		}
	}
	pthread_cond_wait(&retry_cond, &mutex);
}

}


int lock_server_cache_rsm::acquire(int clt, lock_protocol::lockid_t lid, std::string dst, 
             lock_protocol::xid_t xid, int &r)
{

  printf("%s acquire request of lock %016llx\n", dst.c_str(), lid);
  ScopedLock ml(&mutex);
	if (client_lock_id.find(clt) == client_lock_id.end() || client_lock_id[clt].find(lid) == client_lock_id[clt].end()) {
		client_lock_id[clt][lid] = 0;
	}
	if (xid < client_lock_id[clt][lid]) {
		printf("old xid %lld\n", xid);
		return lock_protocol::OK;
	}
	if (xid == client_lock_id[clt][lid]) {
		printf("xid %lld same\n", xid);
		pthread_cond_broadcast(&revoke_cond);
		return lock_protocol::OK;
	}
  nacquire ++;
  r = nacquire;
	// null check
  if (lock_client_list.find(lid) == lock_client_list.end()) {
		std::vector<std::string> client_list;
		lock_client_list[lid] = client_list;
  }

	int position = -1;
	for (int i = 0; i < lock_client_list[lid].size(); i ++) {
		if (lock_client_list[lid].at(i) == dst) {
			position = i;
			break;
		}
	}
	if (position < 0) {
		lock_client_list[lid].push_back(dst);
		position = lock_client_list[lid].size() - 1;
	}
	printf("waiting size %d\n", lock_client_list[lid].size());
	if (lock_client_list[lid].size() > 1) {
		pthread_cond_broadcast(&revoke_cond);
	}
//printf("---------------  %d\n", position);
  printf("%s acquire of lock %016llx succeed\n", dst.c_str(), lid);
	if (position == 0) {
		locks[lid] = true;
		return lock_protocol::OK;
	}
	return lock_protocol::RETRY;
}

int 
lock_server_cache_rsm::release(int clt, lock_protocol::lockid_t lid, std::string dst, 
         lock_protocol::xid_t xid, int &r)
{
  printf("%s release request of lock %016llx\n", dst.c_str(), lid);
  ScopedLock ml(&mutex);
  printf("%s release of lock %016llx succeed\n", dst.c_str(), lid);
  r = nacquire;
	if (xid < client_lock_id[clt][lid]) {
		printf("old xid %lld\n", xid);
		return lock_protocol::OK;
	}
	if (xid == client_lock_id[clt][lid]) {
		printf("xid %lld same\n", xid);
		pthread_cond_broadcast(&retry_cond);
		return lock_protocol::OK;
	}
	client_lock_id[clt][lid] = xid;
  nacquire --;
	if (lock_client_list[lid].size() > 0 && lock_client_list[lid].at(0) == dst) {
		locks[lid] = false;
		lock_client_list[lid].erase(lock_client_list[lid].begin());
	}
	pthread_cond_broadcast(&retry_cond);
  return lock_protocol::OK;
}

std::string
lock_server_cache_rsm::marshal_state()
{
	pthread_mutex_lock(&mutex);
	marshall rep;
	rep << lock_client_list.size();
	std::map<lock_protocol::lockid_t, std::vector<std::string> >::iterator iter;
	for (iter = lock_client_list.begin(); iter != lock_client_list.end(); iter ++) {
		lock_protocol::lockid_t name = iter->first;
		std::vector<std::string> vec = lock_client_list[name];
		rep << name;
		rep << vec;
	}

	rep << locks.size();
	std::map<lock_protocol::lockid_t, bool >::iterator iter2;
	for (iter2 = locks.begin(); iter2 != locks.end(); iter2 ++) {
		lock_protocol::lockid_t name = iter2->first;
		bool value = locks[name];
		rep << name;
		rep << value;
	}

	rep << client_lock_id.size();
	std::map<int, std::map<lock_protocol::lockid_t, lock_protocol::xid_t > >::iterator iter3;
	for (iter3 = client_lock_id.begin(); iter3 != client_lock_id.end(); iter3 ++) {
		int clt = iter3->first;
		rep << clt;
		rep << client_lock_id[clt].size();
		std::map<lock_protocol::lockid_t, lock_protocol::xid_t >::iterator iter4;
		for (iter4 = client_lock_id[clt].begin(); iter4 != client_lock_id[clt].end(); iter4 ++) {
			lock_protocol::lockid_t name = iter4->first;
			lock_protocol::xid_t value = client_lock_id[clt][name];
			rep << name;
			rep << value;
		}
	}

  std::ostringstream ost;
  std::string r;
  pthread_mutex_unlock(&mutex);
  return rep.str();
}

void
lock_server_cache_rsm::unmarshal_state(std::string state)
{
	pthread_mutex_lock(&mutex);
	unmarshall rep(state);
	unsigned int locks_size;
	rep >> locks_size;
	for (unsigned int i = 0; i < locks_size; i ++) {
		lock_protocol::lockid_t name;
		rep >> name;
		std::vector<std::string> vec;
		rep >> vec;
		lock_client_list[name] = vec;
	}

	unsigned int locks_size2;
	rep >> locks_size2;
	for (unsigned int i = 0; i < locks_size2; i ++) {
		lock_protocol::lockid_t name;
		rep >> name;
		bool value;
		rep >> value;
		locks[name] = value;
	}

	unsigned int locks_size3;
	rep >> locks_size3;
	for (unsigned int i = 0; i < locks_size3; i ++) {
		int clt;
		rep >> clt;
		unsigned int locks_size4;
		rep >> locks_size4;
		for (unsigned int j = 0; j < locks_size4; j ++) {
		lock_protocol::lockid_t name;
		rep >> name;
		lock_protocol::xid_t value;
		rep >> value;
		client_lock_id[clt][name] = value;
		}
	}
  pthread_mutex_unlock(&mutex);
}

lock_protocol::status
lock_server_cache_rsm::stat(lock_protocol::lockid_t lid, int &r)
{
  printf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

