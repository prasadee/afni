function [err, V, Info, ErrMessage] = BrikLoad (BrikName, param1, param2)
%
%  OLD USAGE for backward compatibility, see New Usage
%
%   [err, V, Info, ErrMessage] = BrikLoad (BrikName, [form], [MachineFormat])
%
%Purpose:
%   loads an AFNI brik into V
%   
%   
%Input Parameters:
%   BrikName, name of the brik
%   form, 'vector' or 'matrix' this is optional, the default is matrix
%        see .Format help under NEW USAGE
%   MachineFormat is a string such as 'native' or 'ieee-le' (LSB_FIRST) or 'ieee-be' (MSB_FIRST)
%        see .MachineFormat help under NEW USAGE
%
%
%  NEW USAGE
%
%   [err, V, Info, ErrMessage] = BrikLoad (BrikName, [Opt])
%
%Purpose:
%   loads an AFNI brik into V
%   
%   
%Input Parameters:
%   BrikName, name of the brik
%   Opt is an options format with the following optional fields
%   .Format, 'vector' or 'matrix', the default is matrix
%        This determines how the brick data is returned in V.
%        If you choose 'vector' then a N x M x K volume is stored in a 
%        (N * M * K) column vector. If the brick has multiple volumes (4-D)
%        then an N x M x K x J brick is stored in an (N * M * K) x J  matrix.
%        If you use 'vector' option you can change V to matrix format by using
%        M = reshape(V, Info.DATASET_DIMENSIONS(1), Info.DATASET_DIMENSIONS(2),...
%            Info.DATASET_DIMENSIONS(3), Info.DATASET_RANK(2));
%        
%        Note that indexing in matlab is different than in AFNI. Matlab starts indexing at 
%        1 while AFNI starts with 0. So voxel (i,j,k) in AFNI corresponds to voxel (i+1,j+1,k+1) 
%        in matlab. (see example below).
%
%   .MachineFormat is a string such as 'native' or 'ieee-le' (LSB_FIRST) or 'ieee-be' (MSB_FIRST)
%       default is whatever is specified in the .HEAD file.
%       If nothing is specified in the .HEAD file, MSB_FIRST or 'ieee-be' is assumed
%       since most files created with the old versions of AFNI were on the SGIs .
%        You must specify the parameter Opt.Format, to specify Opt.MachineFormat and override the defaults
%       see help fopen for more info 
%   .Scale 0/1 if 1, then the scaling factor is applied to the values read from the brik
%        default is 1
%
% WARNING: If you use .Scale = 0 , you may get bad time series if you have multiple subbriks ! 
%     Each subbrik can have a different scaling factor
%   
%Output Parameters:
%   err : 0 No Problem
%       : 1 Mucho Problems
%   V : the brik data in vector or matrix (X,Y,Z,t) form
%   Info : is a structure containing some Brik infor (from .HEAD file)
% 	 ErrMessage: An error or warning message
%      
%Key Terms:
%   
%More Info :
%   How to read a brick and display a time series:
%   %read the 3D+time brick
%   [err, V, Info, ErrMessage] = BrikLoad ('ARzs_CW_r5+orig'); 
%   %plot the time course of voxel (29, 33, 3)
%   plot (squeeze(V(30 , 34, 4, :))); %squeeze is used to turn the 1x1x1x160 matrix into a 160x1 vector for the plot function
%   
%   
%
%   see also BrikInfo
%
%  Whish list:
%     combine m3dextract with this function and use the skip option in fread
%     to make the function run faster.
%   
%
%     The core for this function (fread and reshape) is based on 
%     Timothy M. Ellmore's (Laboratory of Brain and Cognition, NIMH)
%     function ReadBRIK 
%
%     Author : Ziad Saad
%     Date : Mon Oct 18 14:47:32 CDT 1999 
%     Biomedical Engineering, Marquette University
%     Biophysics Research institute, Medical College of Wisconsin
%


%Define the function name for easy referencing
FuncName = 'BrikLoad';

%Debug Flag
DBG = 1;

%initailize return variables
err = 1;
V = [];
Info = [];
 ErrMessage = '';
 
switch nargin,
	case 1,
		DoVect = 0;
		Opt.MachineFormat = '';
		Opt.Format = 'matrix';
		Opt.Scale = 1;
	case 2,
		if (~isstruct(param1)),
			Opt.Format = param1;
			Opt.MachineFormat = '';
			Opt.Scale = 1;
		else
			Opt = param1;
		end
	case 3,
			Opt.Format = param1;
			Opt.MachineFormat = param2;
			Opt.Scale = 1; 
	otherwise,
	err= ErrEval(FuncName,'Err_Bad option');
	return;
end

%make sure Opt fields are set OK. That's done for the new usage format
%	if (~isfield(Opt,'') | isempty(Opt.)),	Opt. = ; end
	if (~isfield(Opt,'Format') | isempty(Opt.Format)),	Opt.Format = 'matrix'; end
	if (~isfield(Opt,'MachineFormat') | isempty(Opt.MachineFormat)),	Opt.MachineFormat = ''; end
	if (~isfield(Opt,'Scale') | isempty(Opt.Scale)),	Opt.Scale = 1; end %you can change the default for the new usage here


%set the format	
	if (eq_str(Opt.Format,'vector')),
		DoVect = 1;
	elseif(eq_str(Opt.Format,'matrix')),
		DoVect = 0;
	end

%Fix the name of the brik
	%make sure there are no .BRIK or .HEAD	
	vtmp = findstr(BrikName,'.BRIK');
	if (~isempty(vtmp)), %remove .BRIK
		BrikName = BrikName(1:vtmp(1)-1);
	end
	
	vtmp = findstr(BrikName,'.HEAD');
	if (~isempty(vtmp)), %remove .HEAD
		BrikName = BrikName(1:vtmp(1)-1);
	end


	%Now make sure .HEAD and .BRIK are present
	sHead = sprintf('%s.HEAD', BrikName);
	sBRIK = sprintf('%s.BRIK', BrikName);
	if (~filexist(sHead)), 
		ErrMessage = sprintf ('%s: %s not found', FuncName, sHead);
		err = ErrEval(FuncName,'Err_HEAD file not found');
		return;
	end
	if (~filexist(sBRIK)), 
		ErrMessage = sprintf ('%s: %s not found', FuncName, sBRIK);
		err = ErrEval(FuncName,'Err_BRIK file not found');
		return;
	end

	
%get Brik info
	[err, Info] = BrikInfo(BrikName);

if (~strcmp(Info.ByteOrder,'unspecified')),
	%found byte order specs, make sure it does not conflict with the user option
	if (~isempty(Opt.MachineFormat)),
		%user specified a format
		if (~strcmp(Info.ByteOrder,Opt.MachineFormat)),
			%clash, warn
			ErrEval(FuncName,'Wrn_Machine format specified conflicts with .HEAD info. Proceeding with fread ...');
		end
	else
		Opt.MachineFormat = Info.ByteOrder;
	end
else
	%did not find ByteOrder in .HEAD, use user option or pick default
	if (isempty(Opt.MachineFormat)),
		Opt.MachineFormat = 'ieee-be';
		ErrEval(FuncName,'Wrn_No Machine Format was specified by user or found in .HEAD file. Using ieee-be (MSB_FIRST)');
	end
end

%figure out storage type
	itype = unique(Info.BRICK_TYPES);
	if (length(itype) > 1),
		err =  1; ErrMessage = sprintf('Error %s: Not set up to read sub-bricks of multiple sub-types', FuncName); errordlg(ErrMessage);
		return;
	end
	switch itype,
		case 0
			typestr = 'ubit8';
		case 1
			typestr = 'short';
		case 2
			typestr = 'int';
		case 3
			typestr = 'float';
		otherwise
			err = ErrEval(FuncName,'Err_Cannot read this data type');
			return;
	end

%make sure all is well with the Opt.MachineFormat
	
%read .BRIK file
	BrikName = sprintf('%s.BRIK', BrikName);
	
	fidBRIK = fopen(BrikName, 'rb', Opt.MachineFormat);
	if (fidBRIK < 0),
		err = ErrEval(FuncName,'Err_Could not open .BRIK file');
		return;
	end
	V = fread(fidBRIK, (Info.DATASET_DIMENSIONS(1) .* Info.DATASET_DIMENSIONS(2) .* Info.DATASET_DIMENSIONS(3) .* Info.DATASET_RANK(2)) , typestr);
	fclose (fidBRIK);

%scale if required
	if (Opt.Scale),
		iscl = find (Info.BRICK_FLOAT_FACS); %find non zero scales
		NperBrik = Info.DATASET_DIMENSIONS(1) .* Info.DATASET_DIMENSIONS(2) .* Info.DATASET_DIMENSIONS(3);
		if (~isempty(iscl)),
			for j=1:1:length(iscl),
				istrt = 1+ (iscl(j)-1).*NperBrik;
				istp = istrt + NperBrik - 1;
				V(istrt:istp) = V(istrt:istp) .* Info.BRICK_FLOAT_FACS(iscl(j));
			end
		end
	end

%if required, reshape V
	if(~DoVect),
		V = reshape(V, Info.DATASET_DIMENSIONS(1), Info.DATASET_DIMENSIONS(2),...
							Info.DATASET_DIMENSIONS(3), Info.DATASET_RANK(2));
	else
		V = reshape(V, Info.DATASET_DIMENSIONS(1).* Info.DATASET_DIMENSIONS(2).* Info.DATASET_DIMENSIONS(3), Info.DATASET_RANK(2));
	end

	
err = 0;
return;

