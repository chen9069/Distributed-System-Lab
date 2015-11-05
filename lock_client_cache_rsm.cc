// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache_rsm.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

#include "rsm_client.h"

static void *
releasethread(void *x)
{
  lock_client_cache_rsm *cc = (lock_client_cache_rsm *) x;
  cc->releaser();
  return 0;
}

int lock_client_cache_rsm::last_port = 0;

lock_client_cache_rsm::lock_client_cache_rsm(std::string xdst)
  : lock_client(xdst)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache_rsm::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache_rsm::retry_handler);
  xid = 0;
  // You fill this in Step Two, Lab 7
  // - Create rsmc, and use the object to do RPC 
  //   calls instead of the rpcc object of lock_client
	rsmc = new rsm_client(xdst);
  pthread_t th;
	pthread_cond_init(&revoke_cond, NULL);
  int r = pthread_create(&th, NULL, &releasethread, (void *) this);
  VERIFY (r == 0);
}


void
lock_client_cache_rsm::releaser()
{

  // This method should be a continuous loop, waiting to be notified of
  // freed locks that have been revoked by the server, so that it can
  // send a release RPC.

	pthread_mutex_lock(&mutex);
	printf("releaser start\n");
	while (true) {
		printf("%s start releasing", id.c_str());
		std::map<lock_protocol::lockid_t, int>::iterator it = lock_status_list.begin();
		for (; it != lock_status_list.end(); it ++) {
			lock_protocol::lockid_t lid = it->first;
			pthread_cond_t &cond = lock_inUse_cond_list[lid];
  		while (lock_status_list[lid] != FREE && lock_status_list[lid] != NONE) {
				printf("releasing %s wait %llx\n", id.c_str(), lid);
   			pthread_cond_wait(&cond, &mutex);
 		 	}
			if (lock_status_list[lid] == FREE) {
				lock_status_list[lid] = RELEASING;
				printf("releaser %s releasing lock %lld!\n", id.c_str(), lid);
				int r;
				pthread_mutex_unlock(&mutex);
  			lock_protocol::status ret = rsmc->call(lock_protocol::release, cl->id(), lid, id, ++xid, r);
				VERIFY (ret == rlock_protocol::OK);
				pthread_mutex_lock(&mutex);
				printf("releaser %s release %lld succeed!\n", id.c_str(), lid);
    		pthread_cond_t &cond = lock_inUse_cond_list[lid];
				if (lock_status_list[lid] != NONE && lock_status_list[lid] != FREE) {
    			pthread_cond_broadcast(&cond);
				}
				lock_status_list[lid] = NONE;
			}
		}
		pthread_cond_wait(&revoke_cond, &mutex);
	}
// acquire 3 (start from cond signal, start sending rpc)

}


lock_protocol::status
lock_client_cache_rsm::acquire(lock_protocol::lockid_t lid)
{
// revoke 1 np
  pthread_mutex_lock(&mutex); 
	//thread_pool.insert(pthread_self());
	//printf("thread pool size: %d\n", thread_pool.size());
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
 	 	printf("accquire %s send acquire %016llx\n", id.c_str(), lid);
  	lock_protocol::status ret = rsmc->call(lock_protocol::acquire, cl->id(), lid, id, ++xid, r);
// revoke 4 np (lock = ACQUIRING, revoke wait cond ...)
		pthread_mutex_lock(&mutex);
		while (ret == lock_protocol::RETRY) {
			struct timespec ts;
			struct timeval tp;
			ts.tv_sec = tp.tv_sec;
			ts.tv_nsec = tp.tv_usec * 1000;
			ts.tv_sec += 3;
			pthread_cond_t &cond = lock_retry_cond_list[lid];
			while (lock_status_list[lid] != RETRY) {

printf("accquire %s retry wait %llx\n", id.c_str(), lid);
    			//pthread_cond_timedwait(&cond, &mutex, &ts);
					pthread_cond_wait(&cond, &mutex);
			}
			pthread_mutex_unlock(&mutex);
  		ret = rsmc->call(lock_protocol::acquire, cl->id(), lid, id, ++xid, r);
			pthread_mutex_lock(&mutex);
		}
		VERIFY (ret == lock_protocol::OK);
		lock_status_list[lid] = LOCKED;
		pthread_mutex_unlock(&mutex);
// revoke 5 np (lock = LOCKED, revoke wait cond ...)

printf("accquire %s get lock %llx\n", id.c_str(), lid);
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
lock_client_cache_rsm::release(lock_protocol::lockid_t lid)
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
lock_client_cache_rsm::revoke_handler(lock_protocol::lockid_t lid, int &)
{
// acquire 1 (till sending rpc, lock = ACQUIRING)
  //printf("%s get revoke %016llx\n", id.c_str(), lid);
	pthread_mutex_lock(&mutex);
printf("revoke_handler %s get revoke %llx\n", id.c_str(), lid);
	pthread_cond_broadcast(&revoke_cond);
	pthread_mutex_unlock(&mutex);
  return rlock_protocol::OK;
}

rlock_protocol::status
lock_client_cache_rsm::retry_handler(lock_protocol::lockid_t lid, int &)
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


