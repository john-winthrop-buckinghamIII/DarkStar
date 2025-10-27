function NoseConeGenGUI_DarkStar
%Von/Haack Spline CSV for CAD
%Jacob Rizk
%UAH Rocketry TM
%% FIGURE
scr = get(0,'screensize');
W = 1050; H = 620;
f = figure('Name','Nose Cone Generator (CSV in cm)', ...
    'NumberTitle','off','MenuBar','none','ToolBar','none', ...
    'Position',[max(50,(scr(3)-W)/2) max(50,(scr(4)-H)/2) W H], ...
    'Color',[1 1 1]);

ax = axes('Parent',f,'Units','pixels','Position',[430 90 590 500]);
grid(ax,'on'); box(ax,'on'); xlabel(ax,'x (cm)'); ylabel(ax,'y (cm)'); title(ax,'Preview');

uicontrol(f,'Style','text','String','Nose Cone Generator', ...
    'FontSize',16,'FontWeight','bold','BackgroundColor',[1 1 1], ...
    'HorizontalAlignment','left','Position',[20 H-50 400 30]);

%% CONTROLS
x0 = 20; y0 = H-95; dy = 36; wlbl = 170;

mkLabel(f,'Length (L)', x0, y0);                hL    = mkEdit(f,'17', x0+wlbl, y0);
mkLabel(f,'Diameter (D)', x0, y0-dy);           hD    = mkEdit(f,'4.024', x0+wlbl, y0-dy);
mkLabel(f,'Step (dx)', x0, y0-2*dy);            hStep = mkEdit(f,'0.1', x0+wlbl, y0-2*dy);

mkLabel(f,'Units (inputs)', x0, y0-3*dy);
hUnits = uicontrol(f,'Style','popupmenu','String',{'in','mm','cm','m'}, ...
    'Position',[x0+wlbl y0-3*dy 200 26],'BackgroundColor',[1 1 1]);

mkLabel(f,'Profile Type', x0, y0-4*dy);
types = {'Von Karman (C=1/3)','Von Karman (custom C)','Tangent Ogive','Elliptical','Conical','Parabolic (k)'};
hType = uicontrol(f,'Style','popupmenu','String',types, ...
    'Position',[x0+wlbl y0-4*dy 200 26],'BackgroundColor',[1 1 1], ...
    'Callback',@toggleExtraParams);

% Extra parameters
hLblC = mkLabel(f,'Haack C (0..0.333)', x0, y0-5*dy);  hC = mkEdit(f,'0.3333333', x0+wlbl, y0-5*dy);
hLblK = mkLabel(f,'Parabolic power k (>0)', x0, y0-6*dy); hK = mkEdit(f,'1', x0+wlbl, y0-6*dy);

mkLabel(f,'CSV File Name', x0, y0-7*dy);
hFile = mkEdit(f,'NoseCone.csv', x0+wlbl, y0-7*dy);

hIncludeZero = uicontrol(f,'Style','checkbox','String','Include x=0 row', ...
    'Value',1,'BackgroundColor',[1 1 1],'Position',[x0 y0-8*dy 200 24]);

uicontrol(f,'Style','pushbutton','String','Preview','FontWeight','bold', ...
    'Position',[x0 y0-9*dy 140 34],'Callback',@onPreview);

uicontrol(f,'Style','pushbutton','String','Save CSV','FontWeight','bold', ...
    'Position',[x0+160 y0-9*dy 140 34],'Callback',@onSave);

uicontrol(f,'Style','text','String','Notes:','BackgroundColor',[1 1 1], ...
    'HorizontalAlignment','left','Position',[x0 y0-10*dy 320 20]);

noteText = ['Output CSV units: cm (for Fusion).', char(10), ...
            'Inputs L, D, Step use your chosen units; converted to cm.', char(10), ...
            'CSV columns: x, y, z (z=0). Preview mirrors y for display only.'];
uicontrol(f,'Style','text','String',noteText, ...
    'BackgroundColor',[1 1 1],'HorizontalAlignment','left', ...
    'Position',[x0 y0-13*dy 390 3*dy]);

toggleExtraParams(); % init

%% CALLBACKS
    function toggleExtraParams(~,~)
        t = hType.Value;
        if t==2, set([hLblC hC],'Visible','on'); else, set([hLblC hC],'Visible','off'); end
        if t==6, set([hLblK hK],'Visible','on'); else, set([hLblK hK],'Visible','off'); end
    end

    function onPreview(~,~)
        try
            [spline_cm, meta] = generate();   % single-side data
            x = spline_cm(:,1);
            y = spline_cm(:,2);

            cla(ax); hold(ax,'on');
            % Upper profile (to be saved)
            plot(ax, x,  y, 'LineWidth',1.8);
            % Mirrored lower profile (preview only)
            plot(ax, x, -y, 'LineWidth',1.0);

            % Centerline using basic plot (avoid yline for old MATLAB)
            if ~isempty(x)
                xmin = 0;
                xmax = max(x);
                plot(ax, [xmin xmax], [0 0], '-', 'LineWidth', 0.75);
            end

            axis(ax,'equal'); grid(ax,'on'); box(ax,'on');
            xlabel(ax,'x (cm)'); ylabel(ax,'y (cm)');
            ttl = sprintf('%s | L=%.3g %s, D=%.3g %s, step=%.3g %s (Preview: mirrored)', ...
                          meta.typeLabel, meta.L_in, meta.units, meta.D_in, meta.units, meta.step_in, meta.units);
            title(ax, ttl);

            % Symmetric y-lims
            ymax = max(abs(y));
            if isfinite(ymax) && ymax > 0
                ylim(ax, [-1.05*ymax, 1.05*ymax]);
            end
            % X range
            xmax = max(x);
            if isfinite(xmax) && xmax > 0
                xlim(ax, [0, xmax]);
            end
            hold(ax,'off');
        catch ME
            errordlg(ME.message,'Preview Error');
        end
    end

    function onSave(~,~)
        try
            [spline_cm, ~] = generate();  % single-side only
            fname = strtrim(hFile.String);
            if isempty(fname), fname = 'NoseCone.csv'; end
            if numel(fname) < 4 || ~strcmpi(fname(end-3:end), '.csv')
                fname = [fname '.csv'];
            end
            writematrix(spline_cm, fname);
            msgbox(sprintf('Saved %d rows to %s (units: cm)', size(spline_cm,1), fname), 'CSV Saved');
        catch ME
            errordlg(ME.message,'Save Error');
        end
    end

%% CORE
    function [spline_cm, meta] = generate()
        % inputs
        L_in    = str2double(hL.String);
        D_in    = str2double(hD.String);
        step_in = str2double(hStep.String);
        unitStr = hUnits.String{hUnits.Value};
        tIdx    = hType.Value;
        typeStr = types{tIdx};
        incZero = logical(hIncludeZero.Value);

        if ~(isfinite(L_in) && isfinite(D_in) && isfinite(step_in) && L_in>0 && D_in>0 && step_in>0)
            error('L, D, and Step must be positive numbers.');
        end

        % extra params
        if tIdx==2
            C = str2double(hC.String);
            if ~(isfinite(C) && C>=0 && C<=0.5)
                error('C must be between 0 and 0.5 (typical 0..0.333).');
            end
        else
            C = 1/3;
        end

        if tIdx==6
            k = str2double(hK.String);
            if ~(isfinite(k) && k>0)
                error('Parabolic power k must be > 0.');
            end
        else
            k = 1;
        end

        % units -> cm
        scaleToCm = unitScaleToCm(unitStr);
        L = L_in * scaleToCm;
        D = D_in * scaleToCm;
        step = step_in * scaleToCm;
        R = D/2;

        % x grid
        if incZero
            x = (0:step:L).';
            if isempty(x) || x(end) < L-1e-9, x = [x; L]; end
        else
            x = (step:step:L).';
            if isempty(x) || x(end) < L-1e-9, x = [x; L]; end
        end

        % y by type
        y = zeros(size(x));
        switch tIdx
            case 1 % Von Karman C=1/3
                t = acos(1 - 2*x./L);
                y = R * sqrt( (t - 0.5*sin(2*t) + (1/3)*sin(t).^3) / pi );
            case 2 % Von Karman custom C
                t = acos(1 - 2*x./L);
                y = R * sqrt( (t - 0.5*sin(2*t) + C*sin(t).^3) / pi );
            case 3 % Tangent Ogive
                rho = (L^2 + R^2) / (2*R);
                y = sqrt(max(0, rho^2 - (L - x).^2)) + (R - rho);
            case 4 % Elliptical
                t = x./L;
                y = R * sqrt(max(0, 2*t - t.^2));
            case 5 % Conical
                y = (R/L) * x;
            case 6 % Parabolic (k)
                t = x./L;
                y = R * (max(0, 2*t - t.^2)).^k;
            otherwise
                error('Unknown type.');
        end

        y = real(y); y(y<0) = 0;
        z = zeros(size(x));
        spline_cm = [x y z];

        meta = struct('L_in',L_in,'D_in',D_in,'step_in',step_in, ...
                      'units',unitStr,'typeLabel',typeStr,'C',C,'k',k);
    end

end

%% HELPERS
function h = mkLabel(parent,str,x,y)
h = uicontrol(parent,'Style','text','String',str,'BackgroundColor',[1 1 1], ...
    'HorizontalAlignment','left','Position',[x y 170 22]);
end

function h = mkEdit(parent,def,x,y)
h = uicontrol(parent,'Style','edit','String',def,'BackgroundColor',[1 1 1], ...
    'Position',[x y 200 26]);
end

function s = unitScaleToCm(unitStr)
u = lower(unitStr);
if strcmp(u,'in') || strcmp(u,'inch') || strcmp(u,'inches')
    s = 2.54;
elseif strcmp(u,'mm')
    s = 0.1;
elseif strcmp(u,'cm')
    s = 1.0;
elseif strcmp(u,'m') || strcmp(u,'meter') || strcmp(u,'metre') || strcmp(u,'meters') || strcmp(u,'metres')
    s = 100.0;
else
    error('Unsupported unit: %s', unitStr);
end
end
