import os
import tarfile
import shutil
import subprocess
import re
import json
from config import config
from archive import archive_file
import tempfile


def create_ida_job(job_source, local_file, archive_path, archive_type, is_64_bit, compress, logger = None):
    job_desc = {
        "type": "updateprocessor/idaJob",
        "fileName": os.path.basename(local_file),
        "archivePath": archive_path,
        "archiveType": archive_type,
        "is64bit": is_64_bit,
        "compress": compress
    }
    with tempfile.TemporaryDirectory() as job_dir:
        with open(os.path.join(job_dir, "job.json"), "w") as f:
            json.dump(job_desc, f)
        os.symlink(os.path.realpath(local_file), os.path.join(job_dir, os.path.basename(local_file)))
        job_source.create_job(job_dir, logger)


def handle_ida_launch(job_logger, filepath, is_64_bit):
    ida_bin = "idat64" if is_64_bit else "idat"
    ida_bin_path = os.path.join(config["ida_root"], ida_bin)
    dir = os.path.dirname(os.path.realpath(__file__))
    args = [ida_bin_path, "-c", "-A", "-P", "-L/dev/stdout", "-S" + os.path.join(dir, "ida_analysis.py"), filepath]
    job_logger.info(f"Launching IDA: {args}")
    p = subprocess.Popen(args, stdout=subprocess.PIPE)
    while True:
        line = p.stdout.readline()
        if not line:
            break
        line = line.decode('utf-8')
        if line[-1] == '\n':
            line = line[:-1]
        job_logger.info(line)
    if p.wait() != 0:
        raise Exception(f"Failed to launch IDA ({p.returncode})")
    return filepath + (".i64" if is_64_bit else ".idb")

def handle_ida_job(job_source, job_uuid, job_desc, job_dir, job_logger):
    job_logger.info("Starting IDA analysis")
    file_name = os.path.join(job_dir, job_desc["fileName"])
    idb_path = handle_ida_launch(job_logger, file_name, job_desc["is64bit"])
    if job_desc["compress"]:
        job_logger.info("Compressing IDB")
        idb_c_path = os.path.join(job_dir, "archive.tar.xz")
        with tarfile.open(idb_c_path, "w:xz") as tar:
            tar.add(idb_path, arcname=os.path.basename(idb_path))
        job_logger.info("Deleting the IDB")
        os.remove(idb_path)
        job_logger.info("Archiving IDB")
        archive_file(job_desc["archivePath"], idb_c_path, job_desc["archiveType"])
        job_logger.info("Deleting the compressed IDB")
        os.remove(idb_c_path)
    else:
        archive_file(job_desc["archivePath"], idb_path, job_desc["archiveType"])
        job_logger.info("Deleting the IDB")
        os.remove(idb_path)