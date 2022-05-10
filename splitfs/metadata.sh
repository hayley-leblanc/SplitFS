export LEDGER_YCSB=1; make clean; make; cd ../; export LD_LIBRARY_PATH=./splitfs; export NVP_TREE_FILE=./splitfs/bin/nvp_nvp.tree; cd splitfs; 
