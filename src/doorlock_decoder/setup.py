from setuptools import setup
import os
from glob import glob

package_name = 'doorlock_decoder'

setup(
    name=package_name,
    version='0.1.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        # Markers / additional files
        ('share/' + package_name + '/doorlock_decoder', glob('doorlock_decoder/*.py')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='dev',
    maintainer_email='dev@example.com',
    description='H264 video decoder for doorlock (Astra_self_doorlock, Humble)',
    license='MIT',
    entry_points={
        'console_scripts': [
            'decoder_node = doorlock_decoder.video_decoder_node:main',
        ],
    },
)
