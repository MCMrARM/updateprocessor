import subprocess
import os
import shutil
import logging
import json
import concurrent.futures
from threading import Lock, Thread, Event

job_root_logger = logging.getLogger("job")

class SshJobSource:
    def __init__(self, host, remote_root):
        self.host = host
        self.base_cmd = ["ssh", "-t", "-t", host]
        self.remote_root = remote_root

    def _scp_from_remote(self, src, to):
        p = subprocess.run(["scp", "-qr", self.host + ":" + src, to])
        p.check_returncode()

    def pull_job(self, tmp_root):
        cmd = ["cd", self.remote_root]
        cmd = cmd + ["&&", ".", "venv/bin/activate"]
        cmd = cmd + ["&&", "python3", "tool/pull_job.py"]
        p = subprocess.run(self.base_cmd + cmd, capture_output=True, stdin=subprocess.DEVNULL)
        p.check_returncode()
        job = p.stdout.strip().decode('utf-8')
        if len(job) == 0:
            job_root_logger.warning("Received empty job")
            return None
        job_uuid = os.path.basename(job)
        job_logger = logging.getLogger("job." + job_uuid)
        job_logger.info("Received job from remote: " + job)
        tmp_dir = os.path.join(tmp_root, job_uuid)
        os.makedirs(tmp_root)
        if os.path.exists(tmp_dir):
            job_logger.error("Job directory already exists; aborting")
            return None

        job_logger.info("Fetching job files to: " + tmp_dir)
        try:
            self._scp_from_remote(job, tmp_dir)
        except subprocess.SubprocessError:
            job_logger.exception("Failed to fetch job files")
        job_logger.info("Job pull finished")
        return job_uuid, tmp_dir, job_logger

    def ping_job(self, job_uuid):
        cmd = ["cd", self.remote_root]
        cmd = cmd + ["touch", "priv/jobs/active/" + job_uuid]
        p = subprocess.run(self.base_cmd + cmd)
        p.check_returncode()


class JobPingThread(Thread):
    def __init__(self, source):
        super().__init__()
        self.source = source
        self.jobs = set()
        self.lock = Lock()
        self.interval = 60 * 5
        self.stop_event = Event()

    def add_job(self, job_uuid):
        self.lock.acquire()
        self.jobs.add(job_uuid)
        self.lock.release()

    def remove_job(self, job_uuid):
        self.lock.acquire()
        self.jobs.remove(job_uuid)
        self.lock.release()

    def run(self):
        while not self.stop_event.is_set():
            self.lock.acquire()
            if len(self.jobs) > 0:
                job_root_logger.info("Pinging jobs")
            for j in self.jobs:
                try:
                    self.source.ping_job(j)
                except subprocess.SubprocessError:
                    job_root_logger.error("Failed to ping job: " + j)
            self.lock.release()
            self.stop_event.wait(self.interval)

    def stop(self):
        self.stop_event.set()


class JobExecutor:
    def __init__(self, ping_thread):
        self.job_handlers = {}
        self.ping_thread = ping_thread

    def register_job_handler(self, name, handler):
        self.job_handlers[name] = handler

    def execute(self, job_source, job_uuid, job_dir, job_logger):
        job_logger.info("Execution is starting")
        self.ping_thread.add_job(job_uuid)

        try:
            with open(os.path.join(job_dir, "job.json")) as f:
                job_desc = json.load(f)
            job_logger.info("Executing job handler: " + job_desc["type"])
            if job_desc["type"] not in self.job_handlers:
                raise Exception("No matching job execution handler")
            self.job_handlers[job_desc["type"]](job_uuid, job_desc, job_dir, job_logger)

            job_logger.info("Deleting remote job files")
            try:
                raise Exception("not implemented yet")
            except:
                job_logger.exception("Deleting remote job files failed")
        except:
            job_logger.exception("Job execution failed")
        finally:
            job_logger.info("Execution has finished")

            job_logger.info("Deleting local job files")
            try:
                shutil.rmtree(job_dir)
            except:
                job_logger.exception("Deleting local job files failed")

            self.ping_thread.remove_job(job_uuid)
            job_logger.info("Cleanup completed")


class JobPoolExecutor:
    def __init__(self, job_executor, max_jobs = 4):
        self.max_jobs = max_jobs
        self.job_executor = job_executor
        self.futures = []

    def run_main_loop(self, job_source, tmp_root):
        executor = concurrent.futures.ThreadPoolExecutor(max_workers=self.max_jobs)
        try:
            while True:
                if len(self.futures) >= self.max_jobs:
                    concurrent.futures.wait(self.futures, return_when = concurrent.futures.FIRST_COMPLETED)
                    self.futures = [f.running() for f in self.futures]
                job_root_logger.info("Waiting for a job...")
                job_uuid, job_dir, job_logger = job_source.pull_job(tmp_root)
                executor.submit(self.job_executor.execute, job_source, job_uuid, job_dir, job_logger)
        except KeyboardInterrupt:
            pass
        job_root_logger.info("Waiting for running jobs to complete...")
        executor.shutdown()
