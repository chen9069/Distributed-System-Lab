#ifndef __YFS_LOCK__
#define __YFS_LOCK__

#include "lock_client_cache.h"
struct YFSLock {
	private:
		unsigned long long inum;
		lock_client_cache *lc_;
		extent_client *ec_;
	public:
		YFSLock(unsigned long long n, lock_client_cache* lc, extent_client* ec): inum(n), lc_(lc), ec_(ec){
			lc_->acquire(inum);
		}
		~YFSLock() {
			//ec_->flush(inum);
			lc_->release(inum);
		}
};
#endif  /*__YFS_LOCK__*/
