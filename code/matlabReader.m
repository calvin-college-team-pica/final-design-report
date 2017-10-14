% matlabReader.m
% Written by Kendrick, documented by Avery
% in case Avery needs to use it
s = serial('COM1'); % grab COM1
set(s,'BAUD',9600); % set its baudrate
fopen(s); % open it (as though it were a file)
k = 0;
for k=1:100 % collect 100 numbers
    char = fscanf(s, '%u', 1); % get a character from the serial port
    if ~isempty(char)
        charVec(k) = char; % add it to the vector
        % char % DEBUG: print it to console
    end % endif
end % endfor
% now, plot the vector, BUT....
% we have ascii characters starting with 'A', which is 0x65, so fix that offset
% and 'A' was the most negative reading we could get (-0.6V -> 0, 0.6V -> F)
% so adjust so that our 15-character span is centered around 0
plot( single(charVec)-65-7.5 )

fclose(s) % close the file descriptor
delete(s) % remove our link to COM1
clear all % clear the workspace
