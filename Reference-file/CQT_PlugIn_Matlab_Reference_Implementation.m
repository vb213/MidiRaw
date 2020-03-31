%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% ADA - blockbased CQT, Source: Schörkhuber [1]
%
% [1] Ch. Schörkhuber et al., Toolbox for Efficient Perfect Reconstruction
% Time-Frequency Transforms with Log-Frequency Resolution, AES 53rd Int.
% Conf., London, UK, 2014.
%
% READ ME:
% In order for this code to work the Folder 'RUMS' has to be added to the
% matlab path with all subfolders.
% 'RUMS' can be found here: https://git.iem.at/rudrich/rums
%
% Date: 12.11.2019 AS
% Last Update: 25.03.20 PAB => Added Tuning as implemented in JUCE
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
clear variables;
close all;
clc;


set(0,'defaulttextInterpreter','latex')
set(0,'defaultlegendInterpreter','latex')
set(0,'defaultAxesTickLabelInterpreter','latex')


fs = 44100;

% USER PARAMETERSETTINGS

fmin = 110;   % lowest frequency which is analyzed
NOctaves = 5; % Number of analyzed octaves
B = 48;       % Bins per octave e.g B=12 => semitone-resolution
gamma = 0;    % increases Bandwidth for lower frequencies => decreases Q


%% internal parameters derived from User-Settings
overSamplingFactor = 2;
Q = (2^(1 / B) - 2^(-1 / B))^-1; %Q-value for constant Q case
alpha = 1/Q;

fmax = fmin * 2^NOctaves; % highest frequency which is to be analyzed
K = log2(fmax/fmin)*B;  % number of analyzed bins

fm = @(fmin, k, B) 2^(1 / B).^k * fmin; % center-frequency calculation

fk = fm (fmin, [0:K-1], B); % calculate center-frequencies

% apply gamma-value
BOld = alpha.*fk;
Bnew = alpha.*fk+gamma; % new bandwidths are calculated
Qnew = fk./Bnew; % Q-value with gamma decreases for lower-frequencies

Nk_max = round(fs./fmin.*Qnew);
Nk_max = max(Nk_max); % maximum window-size

b_new = log(2)./asinh(0.5./Qnew); % new CQT-resolution in bins per octave

L = 2^nextpow2(Nk_max);
NFFT  = overSamplingFactor * L;

df = fs / NFFT; % Frequency-resolution of big FFT

clear Nk_max
%% number of frequency-points in k-subbands: start-, center-, stop-bin

% calculate start and stop-bins of Windows in frequency domain.
% start-/stop-bins are calculated by subtracting/adding half of the
% corresponding bandwidth from/to the center-bin
% Not completely correct => there are more frequency-bins in the upper-half
% of the window than in the lower half!

% Bk = floor(Bnew/df);

fBinStart = ceil((fk.*2.^(-1./b_new))./df); 
fBinStop = floor((fk.*2.^(1./b_new))./df)+1;
fBinCenter = round(fk./df);

fBinStart(fBinStart<=0)=1;

Bk = fBinStop - fBinStart + 1;

%To see a demonstration of the impact of gamma set gammaDemoFlag to 1
gammaDemoFlag = 0;
if gammaDemoFlag
    % values at the end are gamma-values [0, 3, 10, 24.7/0.108*alpha, 30]
    Bdemo(1,:) = alpha.*fk+0;
    Bdemo(2,:) = alpha.*fk+3;
    Bdemo(3,:) = alpha.*fk+10;
    Bdemo(4,:) = alpha.*fk+24.7/0.108*alpha;
    Bdemo(5,:) = alpha.*fk+30;
    % for frequencies above 500 Hz => appro. consant Q!
    figure
    loglog(fk,Bdemo,'LineWidth',1)
    grid on
    xlim([10 10^4])
    ylim([1 10^3])
    legend('$\gamma = 0$','$\gamma = 3$','$\gamma = 10$',...
           '$\gamma = \Gamma$', '$\gamma = 30$')
    title('Bandwidths for different $\gamma$')
    xlabel('frequency in [Hz]')
    ylabel('Bandwith B in [Hz]')
    axis tight

    figure
    plot(fk,Qnew)
    xlabel('frequency in [Hz]')
    ylabel('Q-value')
    title('Q over center frequencies')
    grid on;

end

%% calculation of IDFT-length and Overlap

Bkmax = max(Bk);       % max. bandwidth - highest subband
M = 2^nextpow2(Bkmax); % IDFT-length

% proportionality-factor between original fs and subband fs,k (k=kmax)
divFact = NFFT/M;

%% calculation of window-functions

% For the constant-Q case (gamma=0) the start- and stop-bins are equally
% distributed on a logarithmic axis. The interpolation points are defined
% by Bk. The window form is a periodic Hann-window

% the windows in frequency-domain are written into the matrix W
W = zeros(NFFT/2+1,K);

% Hann-windows @fk without interpolation:
for k = 1:K
    wtemp = zeros(NFFT/2+1,1);
    if mod(Bk(k), 2)  % odd windowlengths
        w_lower = hann(2 * (fBinCenter(k) - fBinStart(k)) + 1);
        w_upper = hann(2 * (fBinStop(k) - fBinCenter(k)) + 1);
        wtemp(1:Bk(k)) = [w_lower(1:ceil(length(w_lower)/2)); w_upper((ceil(length(w_upper)/2)+1):end)];
    else
        w_lower = hann(2 * (fBinCenter(k) - fBinStart(k) + 1));
        w_upper = hann(2 * (fBinStop(k) - fBinCenter(k)));
        wtemp(1:Bk(k)) = [w_lower(1:ceil(length(w_lower)/2)); w_upper((ceil(length(w_upper)/2)+1):end)];
    end
    
    wtemp = circshift(wtemp,ceil(fk(k)/df)-ceil(Bk(k)/2));
    W(:,k) = wtemp;
end

% There is still potential for improvement here => warped windows would be
% the correct way to design the windows. With warped windows the higher
% number of frequency-bins in the upper-half could be taken into account


%% import audio-data and determine sampling frequency fs


filetype= '*.wav'; % set filetype of audiofile.

[fileName, pathName] = uigetfile({filetype},'Select Audiofile!');

x = AudioData (strcat(pathName,fileName)); % open audio-file
x.data = mean (x.data, 2); % convert to mono

% if sampling frequency is different to fs = 44100 => resample audio.
if x.fs ~= fs
    disp ('Signal resampled')
    x.resample (fs);
end

%% buffering, windowing and zero-padding of imported audio-file
xBlocks = buffer (x, L, L / 2, 'nodelay');
xBlocks.applyWindow (@hann);

% zero-pad the imported audio-file
numZeroPadSamples = (overSamplingFactor - 1) * L;
xBlocks.data{1} = [zeros(numZeroPadSamples / 2, xBlocks.numBlocks);...
        xBlocks.data{1}; zeros(numZeroPadSamples / 2, xBlocks.numBlocks)];

% execute a real valued fft (one-sided).
X = xBlocks.rfft;

%% calculation of CQT-coefficients

% hop size
hs = M / overSamplingFactor / 2;

C = zeros (M - hs, K); % preallocation of coefficient matrix.

for frame =  1 : X.numBlocks
    % apply windows in the frequency-domain
    Xfilt = X.data{1}(:, frame) .* W;
    ifftData = zeros (M, k);
    % extract values between start- and stop-bins => sub-sampling
    for k = 1 : K
        nSmpls = fBinStop(k) - fBinStart(k) + 1;
        fbData = Xfilt(fBinStart(k) : fBinStop(k), k);
        ifftData(1:nSmpls, k) = fbData;
        % Shift shift to base-band is only necessary if icqt is planned
        % => not needed for Analysis-purposes
        % ifftData(:,k) = circshift(ifftData(:,k),-round(nSmpls/2));
    end
    c = abs (ifft (ifftData)); % perform IDFT on the subsampled values
    % overlap and add
    C(end - (M - hs) + 1 : end, :) = C(end - (M - hs) + 1 : end, :) +...
                                     c(1 : M - hs, :);
    C = [C; c(M - hs + 1 : end, :)];
end

%% Tuner as implemented in the plugin:
% Show deviation in cent to tuning frequency.
fTune = 440;
NearestBins = zeros(size(C,1),1);

% get nearest bin to tuning frequency.
minOffset = 100;
for k=1:K
    if abs(fk(k) - fTune) < minOffset
        tuningBinOrig = k;
        minOffset = abs(fk(k)-fTune);
    end
end
tuningBin = tuningBinOrig;

BinsPerSemiTone = B/12;
CSum = zeros(size(C,1),3);

nn=1;
% while loop to replace goto command in JUCE
while nn<size(C,1)
 % matlab index start with 1 therefore tuningBin - 1 to ensure modulo
 % operator works
 modTuning = mod(tuningBin-1,BinsPerSemiTone);
   % Sum Up all corresponding semitone-candidates into 3 bins,  e.g.
   % Sum up every fourth bin for B=48
   for ii = 0:K-1
       if mod(ii,BinsPerSemiTone) == modTuning
        CSum(nn,2)=CSum(nn,2)+C(nn,ii+1);
       end
       if mod(ii,BinsPerSemiTone) == mod((modTuning + 1),BinsPerSemiTone)
        CSum(nn,3)=CSum(nn,3)+C(nn,ii+1);
       end
       if mod(ii,BinsPerSemiTone) == mod((modTuning - 1 + BinsPerSemiTone),BinsPerSemiTone)
        CSum(nn,1)=CSum(nn,1)+C(nn,ii+1);
       end
   end
   % Shift if maximum energy doesn't lie in center-candidate and
   % start calculation again => in JUCE this is done with "goto"
   if CSum(nn,1)>CSum(nn,2)
        tuningBin = tuningBin-1;
        CSum(nn,1:3)=0;
   elseif CSum(nn,3)>CSum(nn,2)
        tuningBin = tuningBin+1;
        CSum(nn,1:3)=0;
   else
    % Apply parabolic interpolation with geometric frequency-bin relation
    % Amplitude Values for parabolic interpolation
       fa = CSum(nn,1);
       fb = CSum(nn,2);
       fc = CSum(nn,3);

    % Frequency Values for parabolic interpolation
       a = fk(tuningBin-1);
       b = fk(tuningBin);
       c = fk(tuningBin+1);
       % source: http://fourier.eng.hmc.edu/e176/lectures/NM/node25.html
       % calculate parabolically interpolated frequency
       fTuneParab(nn) = b + 1/2*(((fa-fb)*(c-b)^2-((fc-fb)*(b-a)^2))/...
                        ((fa-fb)*(c-b)+(fc-fb)*(b-a)));

       nn = nn+1;
       tuningBin = tuningBinOrig;
    end
    nn = max([nn,1]);
end

fTuneParab3 = 0;

%In JUCE the calculated Mistuning is smoothed with some sort of integration
%does not show the same effect in Matlab => Very jumpy start of
%Parab-Interpolated Tuning-frequncy => makes estimation worse

%The Integration code in matlab would look something like:
% integratedTuning = zeros(size(fTuneParab));
% integratedTuning(1)=fTuneParab(1);
% maxTuningCounter = 1024;
% for nn=2:length(fTuneParab)
%     if nn < maxTuningCounter
%         integratedTuning(nn) = fTuneParab(nn)/nn + ...
%                                integratedTuning(nn-1) * (nn - 1)/nn;
%     else
%         integratedTuning(nn) = fTuneParab(nn)/maxTuningCounter +...
%         integratedTuning(nn-1) * (maxTuningCounter - 1)/maxTuningCounter;
%     end
% end
%CentsMisTuneIntegrated = 1200 * log2(integratedTuning./fTune);


CentsMisTune = 1200 * log2(fTuneParab./fTune);

if CentsMisTune > 50
    CentsMisTune = CentsMisTune - 50;
elseif CentsMisTune < -50
    CentsMisTune = CentsMisTune + 50;
end

%% plots

tplot=0:x.duration/(size(C,1)-1):x.duration;
dt = (1:size(C,1))*divFact/fs-NFFT/2/fs;

figure
surf(dt',fk',C','EdgeColor','None'),view(0,90);
set(gca,'YScale','log')
yticks(fmin*(2.^(0:NOctaves)))
ax=gca;
xlim(round([0 tplot(end)]))
axis tight
view([0 90])
colorbar
xlabel('time in [s]')
ylabel('frequency in [Hz]')
title('Absolute-Values of CQT coefficients')


figure
plot(CentsMisTune,'LineWidth',1.5)
grid on
hold on
ylabel(['Mistuning in [cents]'],'LineWidth',1.5)
%ylim([-abs(mean(CentsMisTune))-5 abs(mean(CentsMisTune))+5])
title(['\textbf{Mistuning in respect to ',num2str(fTune), ' Hz}'])
legend('Mistuning')

figure
plot(fTuneParab,'LineWidth',1.5)
grid on
hold on
plot(fTune*ones(size(fTuneParab)),'LineWidth',1.5,'LineStyle','--')
ylim([fTune*2^(-1/B), fTune*2^(1/B)])
ylabel(['frequency in [Hz]'])
title('\textbf{Estimated Tuning-Frequency}')
legend('Estimated Tuning-Frequency','Reference-Tune-Frequency')
