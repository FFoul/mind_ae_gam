
#!/usr/bin/env bash
bench='/home/llh/mind_ae_gam/dht/benchmark'
master="192.168.100.12"
worker="192.168.100.21"
log_dir="/home/llh/mind_ae_gam/log"
master_port=2214
worker_port=2216

mk_dat_dir() {
    for node in $master $worker; do
        ssh $node "if [ ! -d $log_dir ]; then mkdir -p $log_dir; fi"
    done
}

kill_all() {
    for node in $master $worker; do
        ssh $node "sudo killall benchmark"
    done
    sleep 1
}

run_client() {
    local nc=$1
    local nt=$2
    local ratio=$3
    local cid=0
    is_master=1

    # Run the master first
    log_file="$log_dir/$master"_"$nc"_"$nt"_"$ratio"_"$cid".dat
    echo "Run master client at $master"
    cmd="ssh $master \"$bench --is_master $is_master --ip_master $master --ip_worker $master --no_client $nc --get_ratio $ratio --no_thread $nt --client_id --port_worker $worker_port --port_master $master_port $cid 1>$log_file 2>/dev/null &\""
    eval $cmd
    is_master=0
    ((cid++))
    sleep 1

    # Run the worker
    log_file="$log_dir/$worker"_"$nc"_"$nt"_"$ratio"_"$cid".dat
    echo "Run worker client at $worker"
    cmd="ssh $worker \"$bench --is_master $is_master --ip_master $master --ip_worker $worker --no_client $nc --get_ratio $ratio --no_thread $nt --client_id $cid --port_worker $worker_port --port_master $master_port | tee $log_file \""
    eval $cmd
    sleep 1
}


clients=2
# ratios=(100 99 90 50 0)
ratios=(100)
# Create directories on master and worker
mk_dat_dir

for ((thread = 1; thread <= 1; thread++)); do
    for ratio in "${ratios[@]}"; do
        kill_all
        echo "Run benchmark with $clients clients $thread threads and $ratio get_ratio"
        run_client $clients $thread $ratio
    done
done
