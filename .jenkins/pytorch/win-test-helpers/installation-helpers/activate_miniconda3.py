import os
from os.path import exists
import subprocess
import sys


if 'BUILD_ENVIRONMENT' not in os.environ:
    os.environ['CONDA_PARENT_DIR'] = os.environ['CD']
else:
    os.environ['CONDA_PARENT_DIR'] = 'C:\\Jenkins'


# Be conservative here when rolling out the new AMI with conda. This will try
# to install conda as before if it couldn't find the conda installation. This
# can be removed eventually after we gain enough confidence in the AMI

if not exists(conda_parent_dir + '\\Miniconda3'):
    os.environ['INSTALL_FRESH_CONDA'] = 1


install_fresh_conda = os.environ['INSTALL_FRESH_CONDA']
conda_parent_dir = os.environ['CONDA_PARENT_DIR']
tmp_dir_win = os.environ['TMP_DIR_WIN']


if install_fresh_conda == '1':

    try:

        subprocess.run(['curl', '--retry', '3', '-k',
            'https://repo.anaconda.com/miniconda/Miniconda3-latest-Windows-x86_64.exe',
                '--output', tmp_dir_win + '\\Miniconda3-latest-Windows-x86_64.exe'])

        subprocess.run([tmp_dir_win + '\\Miniconda3-latest-Windows-x86_64.exe',
            '/InstallationType=JustMe', '/RegisterPython=0', '/S', '/AddToPath=0',
                '/D=' + conda_parent_dir + '\\Miniconda3'])

    except Exception as e:

        subprocess.run(['echo', 'activate conda failed'])
        subprocess.run(['echo', e])
        sys.exit()


# Activate conda so that we can use its commands, i.e. conda, python, pip
subprocess.run(['call', conda_parent_dir + '\\Miniconda3\\Scripts\\activate.bat',
    conda_parent_dir + '\\Miniconda3'])

if install_fresh_conda == '1':

    try:

        subprocess.run(['call', 'conda', 'install', '-y', '-q', 'numpy"<1.23"',
            'cffi', 'pyyaml', 'boto3', 'libuv'])

        subprocess.run(['call', 'conda', 'install', '-y', '-q', '-c', 'conda-forge', 'cmake=3.22.3'])

    except Exception as e:

        subprocess.run(['echo', 'activate conda failed'])
        subprocess.run(['echo', e])
        sys.exit()
