Import("env", "projenv")
import csv
import os
from subprocess import Popen, PIPE, call
import urllib2
try:
    # for Python 2.x
    from StringIO import StringIO
except ImportError:
    # for Python 3.x
    from io import StringIO

#
# Dump build environment (for debug purpose)
# print env.Dump()
#


def generate_ssl_data(source, target, env):
    print("generate_ssl_data")

    # Mozilla's URL for the CSV file with included PEM certs
    mozurl = "https://ccadb-public.secure.force.com/mozilla/IncludedCACertificateReportPEMCSV"

    # Load the manes[] and pems[] array from the URL
    names = []
    pems = []
    response = urllib2.urlopen(mozurl)
    csvData = response.read()
    csvReader = csv.reader(StringIO(csvData))
    pem_info_index = -1
    for row in csvReader:
        # find the index of the PEM data (its moved at least once)
        if pem_info_index == -1:
            for i in range(0,len(row)):
                if row[i] == "PEM Info":
                    pem_info_index = i
                    break
            if pem_info_index < 0:
                raise RuntimeError("could not find 'PEM Info' index!")
        names.append(row[0]+":"+row[1]+":"+row[2])
        pems.append(row[pem_info_index])
    del names[0] # Remove headers
    del pems[0] # Remove headers

    # Try and make ./data, skip if present
    try:
        os.mkdir("data")
    except:
        pass

    derFiles = []
    idx = 0
    # Process the text PEM using openssl into DER files
    for i in range(0, len(pems)):
        certName = "data/ca_%03d.der" % (idx);
        thisPem = pems[i].replace("'", "")
        print names[i] + " -> " + certName
        ssl = Popen(['openssl','x509','-inform','PEM','-outform','DER','-out', certName], shell = False, stdin = PIPE)
        pipe = ssl.stdin
        pipe.write(thisPem)
        pipe.close()
        ssl.wait()
        if os.path.exists(certName):
            derFiles.append(certName)
            idx = idx + 1

    if os.path.exists("data/certs.ar"):
        os.unlink("data/certs.ar");

    arCmd = '$AR mcs data/certs.ar ' + " ".join(derFiles);
    print(arCmd)
    env.Execute(arCmd)

    for der in derFiles:
        os.unlink(der)


print("Current build targets", map(str, BUILD_TARGETS))

# custom action before building SPIFFS image. For example, compress HTML, etc.
env.AddPreAction("$BUILD_DIR/spiffs.bin", generate_ssl_data)
