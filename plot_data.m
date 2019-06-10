emg = squeeze(data.signals.values())';
sta = status.signals.values(:);

si = size(emg);
dat = [];
for i = 1:si(1)
    if (sta(i) > 0)
    dat = [dat emg(i, 1:sta(i))];
    end
end

plot(dat)
   