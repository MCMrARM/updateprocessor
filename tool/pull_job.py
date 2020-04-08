import os
import sys
import fcntl
from pathlib import Path
import inotify.adapters

lock_file = "priv/jobs/lock"
pending_job_root = "priv/jobs/pending"
active_job_root = "priv/jobs/active"

def try_pick_job():
    f = open(lock_file, "a+")
    fcntl.lockf(f, fcntl.LOCK_EX)

    for p in os.listdir(pending_job_root):
        data_dir = os.readlink(os.path.join(pending_job_root, p))
        data_dir = Path(data_dir).resolve()

        os.symlink(data_dir, os.path.join(active_job_root, p))
        os.remove(os.path.join(pending_job_root, p))

        return data_dir

    fcntl.lockf(f, fcntl.LOCK_UN)
    return None

i = inotify.adapters.Inotify()
i.add_watch(pending_job_root)

job = try_pick_job()
if job is not None:
    print(job)
    sys.exit(0)

for event in i.event_gen():
    job = try_pick_job()
    if job is not None:
        print(job)
        sys.exit(0)

sys.exit(1)