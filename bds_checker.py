from bs4 import BeautifulSoup
from urllib.request import urlopen
from zipfile import ZipFile
from time import sleep
from shutil import copyfileobj
import atexit
import os
import uuid
import json
import re

PAGE = "https://www.minecraft.net/en-us/download/server/bedrock/"

ver = ""
with open("priv/bds_version.txt", "a+") as f:
    f.seek(0)
    ver = f.read()

print("last BDS version: {}".format(ver))


class Job:
    def __init__(self):
        self.uuid = str(uuid.uuid4())
        self.DATA_DIR = "priv/data/" + self.uuid
        self.ACTIVE_DIR = "priv/active/" + self.uuid
        self.PENDING_DIR = "priv/pending/" + self.uuid
        os.mkdir(self.DATA_DIR)

    def write_metadata(self, data):
        data["type"] = "updateprocessor/bdsJob"
        with open(os.path.join(self.DATA_DIR, "job.json"), "w") as f:
            json.dump(data, f)

    def open(self, name, flags="r"):
        return open(os.path.join(self.DATA_DIR, name), flags)

    def activate(self):
        os.symlink(self.DATA_DIR, self.PENDING_DIR)


@atexit.register
def savever():
    with open("priv/bds_version.txt", "w") as f:
        f.write(ver)


def check():
    global ver
    with urlopen(PAGE) as r:
        soup = BeautifulSoup(r, features="html.parser")
        url = soup.find("a", attrs={"data-platform": "serverBedrockLinux"})["href"]
        new_ver = re.match(r".*/bedrock-server-(.*)\.zip", url).group(1)
        if new_ver == ver:
            return
        print("new BDS version: {}".format(new_ver))

        job = Job()
        with job.open("dl.zip", "wb+") as f:
            copyfileobj(urlopen(url), f)

        with ZipFile(job.open("dl.zip", "rb")) as z, \
             job.open("bedrock_server", "wb") as dst, \
             z.open("bedrock_server", "r") as src:
            copyfileobj(src, dst)

        job.write_metadata({})
        job.activate()

        print("job {} active.".format(job.uuid))
        ver = new_ver


while True:
    check()
    sleep(60 * 10)
