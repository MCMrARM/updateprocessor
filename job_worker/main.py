import os
import shutil
import logging
from framework import SshJobSource, JobPingThread, JobExecutor, JobPoolExecutor
from apk_job import handle_add_apk_job
import json

with open("config.json", "r") as f:
    config = json.load(f)

job_root_logger = logging.getLogger("job")
job_root_logger.setLevel(logging.DEBUG)

console_logger = logging.StreamHandler()
console_logger.setLevel(logging.DEBUG)
formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
console_logger.setFormatter(formatter)
job_root_logger.addHandler(console_logger)


tmp_root = config["tmp_dir"]

if os.path.exists(tmp_root):
    shutil.rmtree(tmp_root)

source = SshJobSource(config["remote"]["host"], config["remote"]["root"])
ping_thread = JobPingThread(source)
ping_thread.start()
executor = JobExecutor(ping_thread)
executor.register_job_handler("updateprocessor/addApkJob", handle_add_apk_job)
pool_executor = JobPoolExecutor(executor)
pool_executor.run_main_loop(source, tmp_root)
ping_thread.stop()