// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst, 
				     class extent_client *_ec, class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  rpcs *rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();
	lu = new lock_release_user_cache(cl, id, _ec);
//printf("%s\n", id.c_str());
}

lock_protocol::status 
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
// revoke 1 np
  pthread_mutex_lock(&mutex); 
	//thread_pool.insert(pthread_self());
	//printf("thread pool size: %d\n", thread_pool.size());
  //printf("%s send acquire %016llx\n", id.c_str(), lid);
	// null check
  if (lock_status_list.find(lid) == lock_status_list.end()) {
		lock_status_list[lid] = NONE;
    pthread_cond_t cond;
    lock_inUse_cond_list[lid] = cond;
    pthread_cond_init(&lock_inUse_cond_list[lid], NULL);
    pthread_cond_t retry_cond;
    lock_retry_cond_list[lid] = retry_cond;
    pthread_cond_init(&lock_retry_cond_list[lid], NULL);
  }
	
	// check if the lock is currently in use.
	pthread_cond_t &cond = lock_inUse_cond_list[lid];
  while (lock_status_list[lid] != FREE && lock_status_list[lid] != NONE ) {
    pthread_cond_wait(&cond, &mutex);
// revoke 2 np (lock = FREE, revoke done-> lock = NONE ...)
  }

	// possible status: NONE(new or after revoke), FREE(after release)
	// check if the lock is currently releasing	
	/*
	if (lock_release_cond_list.find(lid) == lock_release_cond_list.end()) {
		pthread_cond_t cond;
		lock_release_cond_list[lid] = cond;
    pthread_cond_init(&lock_release_cond_list[lid], NULL);
	}
	pthread_cond_t &cond = lock_release_cond_list[lid];
	while (lock_status_list[lid] == RELEASING) {
		pthread_cond_wait(&cond, &mutex);
	}
	*/
	if (lock_status_list[lid] == NONE) {
  	lock_protocol::status r;
		lock_status_list[lid] = ACQUIRING;
		pthread_mutex_unlock(&mutex);
// revoke 3 impossible
  	lock_protocol::status ret = cl->call(lock_protocol::acquire, cl->id(), lid, id, r);
// revoke 4 np (lock = ACQUIRING, revoke wait cond ...)
		pthread_mutex_lock(&mutex);
		if (ret == lock_protocol::RETRY) {
			pthread_cond_t &cond = lock_retry_cond_list[lid];
			while (lock_status_list[lid] != RETRY) {
    			pthread_cond_wait(&cond, &mutex);
			}
  		ret = cl->call(lock_protocol::acquire, cl->id(), lid, id, r);
		}
		VERIFY (ret == lock_protocol::OK);
		lock_status_list[lid] = LOCKED;
		pthread_mutex_unlock(&mutex);
// revoke 5 np (lock = LOCKED, revoke wait cond ...)
  	return ret;
	}
	else {
		//printf("lock status(1): %d\n", lock_status_list[lid]);
		lock_status_list[lid] = LOCKED;
		pthread_mutex_unlock(&mutex);
// revoke 6 np (lock = LOCKED, revoke wait cond ...)
		return lock_protocol::OK;
	}
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
	pthread_mutex_lock(&mutex);
  //printf("%s send release %016llx\n", id.c_str(), lid);
	// null check
  if (lock_status_list.find(lid) == lock_status_list.end()) {
    pthread_cond_t cond;
    lock_inUse_cond_list[lid] = cond;
    pthread_cond_init(&lock_inUse_cond_list[lid], NULL);
    pthread_cond_t retry_cond;
    lock_retry_cond_list[lid] = retry_cond;
    pthread_cond_init(&lock_retry_cond_list[lid], NULL);
  }
	// free lock
	pthread_cond_t &cond = lock_inUse_cond_list[lid];
  if (lock_status_list[lid] != NONE && lock_status_list[lid] != FREE) {
    pthread_cond_broadcast(&cond);
  }
	lock_status_list[lid] = FREE;
	//thread_pool.erase(pthread_self());
	pthread_mutex_unlock(&mutex);
	
	return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
// acquire 1 (till sending rpc, lock = ACQUIRING)
	pthread_mutex_lock(&mutex);
  //printf("%s get revoke %016llx\n", id.c_str(), lid);
	pthread_cond_t &cond = lock_inUse_cond_list[lid];
  while (lock_status_list[lid] != FREE && lock_status_list[lid] != NONE) {
    pthread_cond_wait(&cond, &mutex);
  }
	if (lock_status_list[lid] == FREE) {
		lock_status_list[lid] = RELEASING;
		pthread_mutex_unlock(&mutex);
// acquire 2 (till wait cond, lock = RELEASING)
printf("release lock!\n");
  	lu->dorelease(lid);
		pthread_mutex_lock(&mutex);
    pthread_cond_t &cond = lock_inUse_cond_list[lid];
		if (lock_status_list[lid] != NONE && lock_status_list[lid] != FREE) {
    	pthread_cond_broadcast(&cond);
		}
		lock_status_list[lid] = NONE;
		pthread_mutex_unlock(&mutex);
// acquire 3 (start from cond signal, start sending rpc)
  	return rlock_protocol::OK;
	}
	else {
		printf("revoke error: None(null) lock\n");
		return rlock_protocol::RPCERR;
	}
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  //printf("%s get retry %016llx\n", id.c_str(), lid);
	pthread_mutex_lock(&mutex);
	pthread_cond_t &cond = lock_retry_cond_list[lid];
	if (lock_status_list[lid] == ACQUIRING) {
		pthread_cond_broadcast(&cond);
	}
	lock_status_list[lid] = RETRY;
	pthread_mutex_unlock(&mutex);
  return rlock_protocol::OK;
}



