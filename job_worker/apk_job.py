import os
import axmlparserpy.apk as apk
import zipfile
import tarfile
import shutil
import subprocess
import re
from config import config
from archive import archive_file


def handle_ida_launch(job_logger, filepath, arch):
    ida_bin = "idat64" if arch == "arm64-v8a" or arch == "x86_64" else "idat"
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
    return filepath + ".idb"

def handle_add_apk_job(job_uuid, job_desc, job_dir, job_logger):
    apks = [(f["name"], os.path.join(job_dir, f["path"])) for f in job_desc["apks"]]

    main_apk_path = next((path for (name, path) in apks if name == "main"))
    main_apk = apk.APK(main_apk_path)
    version_code = int(main_apk.get_androidversion_code())
    version_name = main_apk.get_androidversion_name()
    job_logger.info(f"Processing APK archive set for version: {version_name} ({version_code})")
    assert version_code == job_desc["versionCode"]

    job_logger.info("Archiving APK archive set")
    archive_base_name = os.path.join(re.match("\d+\.\d+", version_name).group(0) + "x", version_name)
    for (name, path) in apks:
        archive_file(os.path.join(archive_base_name, name + "_" + str(version_code) + ".apk"), path, "apk")

    job_logger.info("Executing IDA for all required files")
    for (apk_name, apk_path) in apks:
        with zipfile.ZipFile(apk_path, 'r') as z:
            for name in z.namelist():
                if name.startswith("lib/") and name.endswith("/libminecraftpe.so"):
                    arch_name = name[4:-len("/libminecraftpe.so")]
                    so_filename = "libminecraftpe_" + str(version_code) + "_" + arch_name
                    extract_path = os.path.join(job_dir, so_filename + ".so")
                    job_logger.info("Extracting native library: {extract_path} from {apk_name}:{name}")
                    with z.open(name) as src, open(extract_path, "wb") as dest:
                        shutil.copyfileobj(src, dest)
                    job_logger.info("Starting IDA analysis")
                    idb_path = handle_ida_launch(job_logger, extract_path, arch_name)
                    job_logger.info("Deleting the extracted file")
                    os.remove(extract_path)
                    job_logger.info("Compressing IDB")
                    idb_c_path = os.path.join(job_dir, so_filename + ".idb.tar.xz")
                    with tarfile.open(idb_c_path, "w:xz") as tar:
                        tar.add(idb_path, arcname=so_filename + ".idb")
                    job_logger.info("Archiving IDB")
                    archive_file(os.path.join(archive_base_name, so_filename + ".idb.tar.xz"), idb_c_path, "idb")
                    job_logger.info("Deleting the IDB")
                    os.remove(idb_c_path)


    pass