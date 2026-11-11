#!/usr/bin/env bash
# Debug-only: collect CernVM-FS (NFS backend) diagnostics on macOS runners.
# Usage: cvmfs_macos_diag.sh <label> [repo ...]
# Output is printed to the log and saved under ${DIAG_DIR:-cvmfs-diag}/<label>/.

label=${1:-diag}
shift
repos=("$@")
[ ${#repos[@]} -eq 0 ] && repos=(cvmfs-config.cern.ch sft.cern.ch sft-nightlies.cern.ch geant4.cern.ch)
out="${DIAG_DIR:-${GITHUB_WORKSPACE:-.}/cvmfs-diag}/${label}"
mkdir -p "${out}"

section() {
  local name=$1
  shift
  echo "::group::[${label}] ${name}"
  {
    echo "### ${name}"
    echo "\$ $*"
    # macOS has no coreutils timeout; guard against hung NFS mounts with perl
    perl -e 'alarm shift; exec @ARGV' 60 bash -c "$*" 2>&1
    echo "(exit code $?)"
  } | tee -a "${out}/${label}.txt"
  echo "::endgroup::"
}

section "date" 'date -u +%FT%TZ; uptime'
section "file descriptor limits (job shell)" 'ulimit -Sn; ulimit -Hn'
section "kernel file limits" 'sysctl kern.maxfiles kern.maxfilesperproc kern.num_files'
section "launchctl limits" 'launchctl limit maxfiles; launchctl limit maxproc'
section "cvmfs2 processes" 'ps -axo pid,ppid,user,etime,rss,vsz,stat,command | grep -E "[c]vmfs2|PID"'
section "open fds per cvmfs2 process" 'for p in $(pgrep cvmfs2); do printf "%s %s\n" "$p" "$(lsof -p "$p" 2>/dev/null | wc -l)"; done'
section "nfs mounts" 'mount | grep -E "cvmfs|nfs"'
section "nfsstat -m" 'nfsstat -m'
section "nfsstat client" 'nfsstat -c'
section "/cvmfs and compat paths" 'ls -la /cvmfs /Users/Shared/cvmfs /Volumes/cvmfs'
section "default.local" 'cat /etc/cvmfs/default.local'
section "effective NFS-relevant parameters" 'cvmfs_config showconfig sft.cern.ch | grep -E "CVMFS_(NFILES|MOUNT_BACKEND|NFS|USER|CACHE_BASE|QUOTA|RELOAD|DEBUGLOG|USYSLOG|SYSLOG)"'
section "cvmfs_config stat -v" 'cvmfs_config stat -v'
for r in "${repos[@]}"; do
  section "cvmfs_talk ${r} open files / eio counters" "cvmfs_talk -i ${r} internal affairs | grep -i -E 'eio|emfile|nfs|open|fd|stateid|no_space|catalog_mgr.n_listing' | head -80"
  section "cvmfs_talk ${r} host/proxy info" "cvmfs_talk -i ${r} host info; cvmfs_talk -i ${r} proxy info"
  section "ls ${r}" "ls /cvmfs/${r} | head -5"
done
section "cache usage" 'du -sh ~/Library/Caches/cvmfs/* 2>/dev/null; df -h ~/Library/Caches'
section "crash reports" 'ls -la ~/Library/Logs/DiagnosticReports /Library/Logs/DiagnosticReports 2>/dev/null | grep -i -E "cvmfs|mount|total"'
for f in ~/Library/Logs/DiagnosticReports/*cvmfs* /Library/Logs/DiagnosticReports/*cvmfs*; do
  [ -f "$f" ] && cp "$f" "${out}/" && section "crash report $(basename "$f")" "head -150 '$f'"
done
# Full unified logs go to files only (too long for the job log); print a tail.
log show --last 2h --info --debug --style compact \
  --predicate 'process == "cvmfs2" OR process == "mount_cvmfs" OR eventMessage CONTAINS[c] "cvmfs"' \
  > "${out}/unified-log-cvmfs.txt" 2>&1
log show --last 2h --info --style compact \
  --predicate 'process == "kernel" AND (eventMessage CONTAINS[c] "nfs" OR eventMessage CONTAINS[c] "127.0.0.1")' \
  > "${out}/unified-log-kernel-nfs.txt" 2>&1
section "unified log: cvmfs (last 200 lines)" "tail -n 200 '${out}/unified-log-cvmfs.txt'"
section "unified log: kernel nfs (last 100 lines)" "tail -n 100 '${out}/unified-log-kernel-nfs.txt'"
exit 0
