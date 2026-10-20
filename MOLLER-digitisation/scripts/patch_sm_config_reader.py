#!/usr/bin/env python3
"""Add only the two new SM configuration keys to an existing driver."""
from pathlib import Path
import sys
path=Path(sys.argv[1] if len(sys.argv)>1 else 'src/mollerdig.cxx')
s=path.read_text()
keys=['readout_all_channels_showermax','calibration_file_showermax']
if all(('if(skey=="'+k+'")') in s for k in keys):
 print('SM configuration keys are already present; no change');sys.exit(0)
if any(k in s for k in keys):
 raise SystemExit('Partial SM configuration update found; inspect manually before patching')
anchor='if(skey=="store_waveforms_showermax")'
if s.count(anchor)!=1:
 raise SystemExit('Expected one existing store_waveforms_showermax handler; no file changed')
pos=s.index(anchor)
new='''if(skey=="readout_all_channels_showermax") { showerMaxCfg.readout_all_channels = ((TObjString*)(*tokens)[1])->GetString().Atoi()!=0; }
if(skey=="calibration_file_showermax") { showerMaxCfg.calibration_file = ((TObjString*)(*tokens)[1])->GetString().Data(); }
'''
backup=path.with_name(path.name+'.before_sm_pedestal')
if backup.exists():raise SystemExit('Backup already exists; no file changed')
backup.write_bytes(path.read_bytes())
path.write_text(s[:pos]+new+s[pos:])
print('Updated '+str(path)+'; original saved as '+str(backup))
