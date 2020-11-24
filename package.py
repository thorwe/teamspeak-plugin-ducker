import os
import shutil
import zlib
import zipfile

archs = ['32', '64']

BUILD_PATH_PRE = "build-teamspeak-plugin-ducker-Desktop_Qt_5_12_10_MSVC2017_"
BUILD_PATH_POST = "bit_2017-RelWithDebInfo"

def copy_file(src, dest):
    print("copying {0} to {1}".format(src, dest))
    try:
        shutil.copy(src, dest)
    # eg. src and dest are the same file
    except shutil.Error as e:
        print('Error: %s' % e)
    # eg. source or destination doesn't exist
    except IOError as e:
        print('Error: %s' % e.strerror)


def compress(file_names, arch):
    print("File Paths:")
    print(file_names)

    path = "./"

    # Select the compression mode ZIP_DEFLATED for compression
    # or zipfile.ZIP_STORED to just store the file
    compression = zipfile.ZIP_DEFLATED

    # create the zip file first parameter path/name, second mode
    zf = zipfile.ZipFile("ducker_plugin_win" + arch + ".ts3_plugin", mode="w")
    try:
        for file_name in file_names:
            # Add file to the zip file
            # first parameter file to zip, second filename in zip
            zf.write(path + file_name,
                     file_names[file_name], compress_type=compression)

    except OSError:
        print("An error occurred")
    finally:
        # Don't forget to close the file!
        zf.close()


def updatePackageIni(packageIniPath, build_path, arch):
    copy_file(packageIniPath, build_path)
    try:
        with open(os.path.join(build_path, "package.ini"), "a") as f:
            f.write('Platforms = win{}\r\n'.format(arch))
    except:
        print("An error occurred")



for arch in archs:
    print("--------------------")
    print("Building arch {0}".format(arch))
    build_path = os.path.join("..", BUILD_PATH_PRE + arch + BUILD_PATH_POST)
    updatePackageIni(os.path.join("res", "package.ini"), build_path, arch)
    file_names = {
        os.path.join(build_path, "package.ini"): "package.ini",
        os.path.join("res", "duck_16.png"): os.path.join("plugins", "ducker_plugin", "duck_16.png"),
        os.path.join(build_path, "Ducker.dll"): os.path.join("plugins", "ducker_plugin_win" + arch + ".dll")
    }
    compress(file_names, arch)
