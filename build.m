% Buinitialize structure
def = struct([]);
curPath = pwd;
% generate TMSI block files
def{1} = legacy_code('initialize');
def{1}.SFunctionName = 'TMSI';
def{1}.StartFcnSpec  = strcat('void init_tmsi(void** work1, uint16 p1, int32 p2, ', ...
                              'int32 p3, int32 p4, int32 p5[], uint32 size(p5, 1))');
def{1}.OutputFcnSpec = 'void output_tmsi(void** work1, double y1[size(p5, 1)][p4], int32 y2[1])';
def{1}.TerminateFcnSpec = 'terminate_tmsi(void** work1)';
def{1}.IncPaths = {strcat(curPath)};
def{1}.SrcPaths = {strcat(curPath)};
def{1}.HeaderFiles   = {'tmsi.hpp','tmsiDevice.h'};
def{1}.SourceFiles   = {'tmsiDevice.cpp'};
def{1}.Options.language = 'C++';
def{1}.TargetLibFiles = {'libusb-1.0.so'};
def{1}.HostLibFiles = {'libusb-1.0.so'};
def{1}.SampleTime = 'parameterized';
def{1}.LibPaths = {'/usr/lib/x86_64-linux-gnu/'};
def{1}.Options.useTlcWithAccel = false;
legacy_code('sfcn_tlc_generate', def{1});
legacy_code('generate_for_sim', def{1});

% generate rtwmakecfg script
legacy_code('rtwmakecfg_generate', cell2mat(def));
