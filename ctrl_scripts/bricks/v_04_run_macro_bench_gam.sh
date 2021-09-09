#!/bin/bash
#$1: node id
#$2: number of nodes
#$3: number of thread per blade
#$4: trace type = [tf, gc, ma, mc]
#$5: maximum step (workload specific)
#$6: controller ip address
#$7: worker ip address
#$8: controller port number
#$9: worker port number
#$10: trace type full name

if [ $# -le 9 ]; then
  echo "Error: missing arguments: $@"
fi

LOG_DIR=~/Downloads/04_macro_bench_$4
mkdir -p $LOG_DIR
echo "Run for Node: $1"
source g_set_env_gam.sh

NFS_IP="192.168.122.1"
echo $NFS_IP

# Mount logs
sudo mkdir -p /test_program_log
sudo mkdir -p /test_program_log/${4}
sudo mount ${NFS_IP}:/media/data_ssds/${10}_logs /test_program_log/${4}
sudo mkdir -p /media/data_raid0/
sudo mkdir -p /media/data_raid0/${10}_logs
sudo mount ${NFS_IP}:/media/data_raid0/${10}_logs /media/data_raid0/${10}_logs

cd ~/mind_ae_gam/lib/libcuckoo && autoreconf -fis && ./configure && make && sudo make install && cd ~/mind_ae_gam/src && make -j 8 && cd ~/mind_ae_gam/test && make -j 8

cd ${GAM_PATH}/test/
pwd
make_cmd="./gam_profile_test"
echo "Make cmd: ${make_cmd}"
if [[ $1 -eq 0 ]]; then
  echo $make_cmd $2 $3 $6 $7 $8 $9 1 1 $5 /test_program_log/${4}/0 /test_program_log/${4}/1 /test_program_log/${4}/2 /test_program_log/${4}/3 /test_program_log/${4}/4 /test_program_log/${4}/5 /test_program_log/${4}/6 /test_program_log/${4}/7 /test_program_log/${4}/8 /test_program_log/${4}/9_0
  $make_cmd $2 $3 $6 $7 $8 $9 1 1 $5 /test_program_log/${4}/0 /test_program_log/${4}/1 /test_program_log/${4}/2 /test_program_log/${4}/3 /test_program_log/${4}/4 /test_program_log/${4}/5 /test_program_log/${4}/6 /test_program_log/${4}/7 /test_program_log/${4}/8 /test_program_log/${4}/9_0 &> result.log &
  $make_cmd $2 $3 $6 $7 $8 $9 1 1 $5 /test_program_log/${4}/${10}_0_0 /test_program_log/${4}/${10}_1_0 /test_program_log/${4}/${10}_2_0 /test_program_log/${4}/${10}_3_0 /test_program_log/${4}/${10}_4_0 /test_program_log/${4}/${10}_5_0 /test_program_log/${4}/${10}_6_0 /test_program_log/${4}/${10}_7_0 /test_program_log/${4}/${10}_8_0 /test_program_log/${4}/${10}_9_0 &> result.log &
elif [[ $1 -eq 1 ]]; then
  echo $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /media/data_raid0/${10}_logs/${10}_10_0 /media/data_raid0/${10}_logs/${10}_11_0 /media/data_raid0/${10}_logs/${10}_12_0 /media/data_raid0/${10}_logs/${10}_13_0 /media/data_raid0/${10}_logs/${10}_14_0 /media/data_raid0/${10}_logs/${10}_15_0 /media/data_raid0/${10}_logs/${10}_16_0 /media/data_raid0/${10}_logs/${10}_17_0 /media/data_raid0/${10}_logs/${10}_18_0  /media/data_raid0/${10}_logs/${10}_19_0
  $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /media/data_raid0/${10}_logs/${10}_10_0 /media/data_raid0/${10}_logs/${10}_11_0 /media/data_raid0/${10}_logs/${10}_12_0 /media/data_raid0/${10}_logs/${10}_13_0 /media/data_raid0/${10}_logs/${10}_14_0 /media/data_raid0/${10}_logs/${10}_15_0 /media/data_raid0/${10}_logs/${10}_16_0 /media/data_raid0/${10}_logs/${10}_17_0 /media/data_raid0/${10}_logs/${10}_18_0  /media/data_raid0/${10}_logs/${10}_19_0 &> result.log &

elif [[ $1 -eq 2 ]]; then
  echo $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /test_program_log/${4}/${10}_20_0 /test_program_log/${4}/${10}_21_0 /test_program_log/${4}/${10}_22_0 /test_program_log/${4}/${10}_23_0 /test_program_log/${4}/${10}_24_0 /test_program_log/${4}/${10}_25_0 /test_program_log/${4}/${10}_26_0 /test_program_log/${4}/${10}_27_0 /test_program_log/${4}/${10}_28_0 /test_program_log/${4}/${10}_29_0
  $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /test_program_log/${4}/${10}_20_0 /test_program_log/${4}/${10}_21_0 /test_program_log/${4}/${10}_22_0 /test_program_log/${4}/${10}_23_0 /test_program_log/${4}/${10}_24_0 /test_program_log/${4}/${10}_25_0 /test_program_log/${4}/${10}_26_0 /test_program_log/${4}/${10}_27_0 /test_program_log/${4}/${10}_28_0 /test_program_log/${4}/${10}_29_0 &> result.log &

elif [[ $1 -eq 3 ]]; then
  echo $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /media/data_raid0/${10}_logs/${10}_30_0 /media/data_raid0/${10}_logs/${10}_31_0 /media/data_raid0/${10}_logs/${10}_32_0 /media/data_raid0/${10}_logs/${10}_33_0 /media/data_raid0/${10}_logs/${10}_34_0 /media/data_raid0/${10}_logs/${10}_35_0 /media/data_raid0/${10}_logs/${10}_36_0 /media/data_raid0/${10}_logs/${10}_37_0 /media/data_raid0/${10}_logs/${10}_38_0  /media/data_raid0/${10}_logs/${10}_39_0
  $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /media/data_raid0/${10}_logs/${10}_30_0 /media/data_raid0/${10}_logs/${10}_31_0 /media/data_raid0/${10}_logs/${10}_32_0 /media/data_raid0/${10}_logs/${10}_33_0 /media/data_raid0/${10}_logs/${10}_34_0 /media/data_raid0/${10}_logs/${10}_35_0 /media/data_raid0/${10}_logs/${10}_36_0 /media/data_raid0/${10}_logs/${10}_37_0 /media/data_raid0/${10}_logs/${10}_38_0  /media/data_raid0/${10}_logs/${10}_39_0

elif [[ $1 -eq 4 ]]; then
  echo $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /test_program_log/${4}/${10}_40_0 /test_program_log/${4}/${10}_41_0 /test_program_log/${4}/${10}_42_0 /test_program_log/${4}/${10}_43_0 /test_program_log/${4}/${10}_44_0 /test_program_log/${4}/${10}_45_0 /test_program_log/${4}/${10}_46_0 /test_program_log/${4}/${10}_47_0 /test_program_log/${4}/${10}_48_0 /test_program_log/${4}/${10}_49_0
  $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /test_program_log/${4}/${10}_40_0 /test_program_log/${4}/${10}_41_0 /test_program_log/${4}/${10}_42_0 /test_program_log/${4}/${10}_43_0 /test_program_log/${4}/${10}_44_0 /test_program_log/${4}/${10}_45_0 /test_program_log/${4}/${10}_46_0 /test_program_log/${4}/${10}_47_0 /test_program_log/${4}/${10}_48_0 /test_program_log/${4}/${10}_49_0 &> result.log &

elif [[ $1 -eq 5 ]]; then
  echo $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /media/data_raid0/${10}_logs/${10}_50_0 /media/data_raid0/${10}_logs/${10}_51_0 /media/data_raid0/${10}_logs/${10}_52_0 /media/data_raid0/${10}_logs/${10}_53_0 /media/data_raid0/${10}_logs/${10}_54_0 /media/data_raid0/${10}_logs/${10}_55_0 /media/data_raid0/${10}_logs/${10}_56_0 /media/data_raid0/${10}_logs/${10}_57_0 /media/data_raid0/${10}_logs/${10}_58_0  /media/data_raid0/${10}_logs/${10}_59_0
  $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /media/data_raid0/${10}_logs/${10}_50_0 /media/data_raid0/${10}_logs/${10}_51_0 /media/data_raid0/${10}_logs/${10}_52_0 /media/data_raid0/${10}_logs/${10}_53_0 /media/data_raid0/${10}_logs/${10}_54_0 /media/data_raid0/${10}_logs/${10}_55_0 /media/data_raid0/${10}_logs/${10}_56_0 /media/data_raid0/${10}_logs/${10}_57_0 /media/data_raid0/${10}_logs/${10}_58_0  /media/data_raid0/${10}_logs/${10}_59_0 &> result.log &

elif [[ $1 -eq 6 ]]; then
  echo $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /test_program_log/${4}/${10}_60_0 /test_program_log/${4}/${10}_61_0 /test_program_log/${4}/${10}_62_0 /test_program_log/${4}/${10}_63_0 /test_program_log/${4}/${10}_64_0 /test_program_log/${4}/${10}_65_0 /test_program_log/${4}/${10}_66_0 /test_program_log/${4}/${10}_67_0 /test_program_log/${4}/${10}_68_0 /test_program_log/${4}/${10}_69_0
  $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /test_program_log/${4}/${10}_60_0 /test_program_log/${4}/${10}_61_0 /test_program_log/${4}/${10}_62_0 /test_program_log/${4}/${10}_63_0 /test_program_log/${4}/${10}_64_0 /test_program_log/${4}/${10}_65_0 /test_program_log/${4}/${10}_66_0 /test_program_log/${4}/${10}_67_0 /test_program_log/${4}/${10}_68_0 /test_program_log/${4}/${10}_69_0 &> result.log &

elif [[ $1 -eq 7 ]]; then
  echo $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /media/data_raid0/${10}_logs/${10}_70_0 /media/data_raid0/${10}_logs/${10}_71_0 /media/data_raid0/${10}_logs/${10}_72_0 /media/data_raid0/${10}_logs/${10}_73_0 /media/data_raid0/${10}_logs/${10}_74_0 /media/data_raid0/${10}_logs/${10}_75_0 /media/data_raid0/${10}_logs/${10}_76_0 /media/data_raid0/${10}_logs/${10}_77_0 /media/data_raid0/${10}_logs/${10}_78_0  /media/data_raid0/${10}_logs/${10}_79_0
  $make_cmd $2 $3 $6 $7 $8 $9 0 1 $5 /media/data_raid0/${10}_logs/${10}_70_0 /media/data_raid0/${10}_logs/${10}_71_0 /media/data_raid0/${10}_logs/${10}_72_0 /media/data_raid0/${10}_logs/${10}_73_0 /media/data_raid0/${10}_logs/${10}_74_0 /media/data_raid0/${10}_logs/${10}_75_0 /media/data_raid0/${10}_logs/${10}_76_0 /media/data_raid0/${10}_logs/${10}_77_0 /media/data_raid0/${10}_logs/${10}_78_0  /media/data_raid0/${10}_logs/${10}_79_0 &> result.log &

# Memory blade id start from 16
elif [[ $1 -eq 16 ]]; then
  echo $make_cmd 2 4 $6 $7 $8 $9 0 0 100 ~/tensorflow_0_0 ~/tensorflow_1_0 ~/tensorflow_2_0 ~/tensorflow_3_0
  $make_cmd 2 4 $6 $7 $8 $9 0 0 100 ~/tensorflow_0_0 ~/tensorflow_1_0 ~/tensorflow_2_0 ~/tensorflow_3_0
fi
sleep 30
