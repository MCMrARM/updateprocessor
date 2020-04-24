import os
import axmlparserpy.apk as apk
import zipfile
import shutil
import re
from archive import archive_file
from ida_job import create_ida_job

def handle_add_apk_job(job_source, job_uuid, job_desc, job_dir, job_logger):
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
                    job_logger.info(f"Extracting native library: {extract_path} from {apk_name}:{name}")
                    with z.open(name) as src, open(extract_path, "wb") as dest:
                        shutil.copyfileobj(src, dest)

                    job_logger.info("Creating IDA job")
                    is_64_bit = arch_name == "arm64-v8a" or arch_name == "x86_64"
                    archive_path = os.path.join(archive_base_name, so_filename + (".i64" if is_64_bit else ".idb") + ".tar.xz")
                    create_ida_job(job_source, extract_path, archive_path, "apk_idb", is_64_bit, True)

                    job_logger.info("Deleting the extracted file")
                    os.remove(extract_path)


    pass