from config import config
import os
import shutil


class ArchiveConfig:
    def __init__(self, config):
        self.exclude = set(config["exclude"]) if "exclude" in config else set()

class FilesystemArchiveConfig(ArchiveConfig):
    def __init__(self, config):
        super().__init__(config)
        self.path = config["path"]

    def execute(self, file_name, file_path):
        path = os.path.join(self.path, file_name)
        if not os.path.exists(os.path.dirname(path)):
            os.makedirs(os.path.dirname(path))
        shutil.copyfile(file_path, path)


def create_archive_config(config):
    if config["type"] == "filesystem":
        return FilesystemArchiveConfig(config)
    raise Exception("Invalid archive config type: " + config["type"])


archive_config = [create_archive_config(cfg) for cfg in config["archive"]]


def archive_file(file_name, file_path, file_type):
    for ac in archive_config:
        if file_type in ac.exclude:
            continue
        ac.execute(file_name, file_path)
