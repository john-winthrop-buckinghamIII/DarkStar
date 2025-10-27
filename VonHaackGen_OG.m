%Von/Haack Spline CSV for CAD
%Jacob Rizk
%UAH Rocketry TM

%% House Keeping
clear
clc

%% User Inputs
L = 16*2.54; %converts to inches
D = 4.024*2.54;
C = (1/3);
step = 0.1;
file = "HaackCone16in.csv";

%% Code Variables
x = 0;
y = 0;
z = 0;
spline = zeros(0, 3);
i = 1;

%% Generator
x = x + step; % spline already has a row of 0, so this starts it off at next x value.

while x <= L
    i = i + 1;
    t_x = acos(1-2*x/L);
    y = (D/2)*(sqrt((t_x-0.5*sin(2*t_x)+C*sin(t_x)^3)/pi));
    spline(i,:) = [x,y,z];
    x = x + step;
end

spline = real(spline);
%% Display Results
disp(spline);
writematrix(spline, file);
fprintf('Saved spline to %s\n', file);
disp("VonHaackGenerator");