function [header, events] = read_es(filename)
%READ_ES Read Event Stream files with Matlab standalone
%
%   [HEADER, EVENTS] = READ_EVENT_STREAM(FILENAME) reads the Event Stream file
%   as described in [1] and returns the header data in HEADER, containing
%   information about the stream type ; and also the event stream
%   as a struct of fields containing 1*N vector, where N is the number of
%   events.
%   The first field of the event structure is t, containing the timestamp
%   of each events.
%
%   This implementation is for EVENT STREAM VERSION 2.0
%
%   References :
%   [1] https://github.com/neuromorphic-paris/event_stream/tree/cc64e09f9c9650955fac94195c71ac27b6e2509f



fid = fopen(filename);
if fid == -1
  error(['The file ', filename, ' could not be open for reading']);
end

spec = fread(fid,12,'uint8=>char')';
is_eventstream = strcmp(spec, 'Event Stream');

if ~is_eventstream
  error('This file is not in the Event Stream format.')
end

major_version = fread(fid,1,'*uint8');
minor_version = fread(fid,1,'*uint8');
patch_version = fread(fid,1,'*uint8');


stream_types_v2 = uint8([0,1,2,3,4]);
stream_types_v2_str = {'generic', 'dvs', 'atis', ...
'asynchronous and modular display', 'color'};

steam_type_byte = fread(fid,1,'*uint8');
header.type = stream_types_v2_str{stream_types_v2==steam_type_byte};

% Some code are redundant but permits to gain speed (without conditional branching)
switch header.type
  case 'generic'
    data = fread(fid,Inf,'*uint8');
    fclose(fid);
    nev_max = floor(numel(data)/2);
    t = zeros(nev_max,1,'uint64');
    bytes = strings(nev_max,1);

    nev = 0;
    overflow_check = 254;
    overflow_bytes = 0;
    last_t = 0;
    ind = 1;

    while ind<numel(data)
      if(data(ind)<overflow_check)
        nev = nev + 1;
        t(nev) = uint64(data(ind))+254*overflow_bytes+last_t;
        nbbytes = 1;
        total_size_byte = bitshift(uint64(data(ind+nbbytes)), -1);
        is_last = bitget(data(ind+nbbytes),1);
        while(is_last)
          nbbytes = nbbytes + 1;
          total_size_byte = total_size_byte + bitshift(uint64(data(ind+nbbytes)), -1+7*(nbbytes-1));
          is_last = bitget(data(ind+nbbytes),1);
        end
        bytes(nev) = char(data(ind+nbbytes+(1:total_size_byte))');
        last_t = t(nev);
        ind = ind + nbbytes + total_size_byte + 1;
        overflow_bytes = 0;
      elseif (data(ind) == overflow_check)
        overflow_bytes = 0;
        ind = ind + 1;
      else
        overflow_bytes = overflow_bytes + 1;
        ind = ind + 1;
      end
    end
    events.t = t(1:nev);
    events.bytes = bytes(1:nev);


  case 'dvs'
    header.width = fread(fid,1,'*uint16');
    header.height = fread(fid,1,'*uint16');
    data = fread(fid,Inf,'*uint8');
    fclose(fid);

    nev_max = floor(numel(data)/5);
    t = zeros(nev_max,1,'uint64');
    x = zeros(nev_max,1,'uint16');
    y = zeros(nev_max,1,'uint16');
    is_increase = zeros(nev_max,1,'logical');
    overflow_check = 254;
    nbbytes = 5;

    nev = 0;
    overflow_bytes = 0;
    last_t = 0;
    ind = 1;
    while ind<numel(data)
      if(data(ind)<overflow_check)
        nev = nev + 1;
        [t(nev), x(nev), y(nev), is_increase(nev)] = ...
          bytes_dvs_to_event(data(ind+(0:(nbbytes-1))), overflow_bytes, last_t);
        last_t = t(nev);
        ind = ind + nbbytes;
        overflow_bytes = 0;
      elseif (data(ind) == overflow_check)
        overflow_bytes = 0;
        ind = ind + 1;
      else
        overflow_bytes = overflow_bytes + 1;
        ind = ind + 1;
      end
    end
    events.t = t(1:nev);
    events.x = x(1:nev);
    events.y = y(1:nev);
    events.is_increase = is_increase(1:nev);

  case 'atis'
    header.width = fread(fid,1,'*uint16');
    header.height = fread(fid,1,'*uint16');
    data = fread(fid,Inf,'*uint8');
    fclose(fid);

    nev_max = floor(numel(data)/5);
    t = zeros(nev_max,1,'uint64');
    x = zeros(nev_max,1,'uint16');
    y = zeros(nev_max,1,'uint16');
    is_threshold_crossing = zeros(nev_max,1,'logical');
    polarity = zeros(nev_max,1,'logical');
    overflow_check = 252;
    overflow_bytes_check = 3;
    nbbytes = 5;

    nev = 0;
    overflow_bytes = 0;
    last_t = 0;
    ind = 1;
    while ind<numel(data)
      if(data(ind)<overflow_check)
        nev = nev + 1;
        [t(nev), x(nev), y(nev), is_threshold_crossing(nev), polarity(nev)] = ...
          bytes_atis_to_event(data(ind+(0:(nbbytes-1))), overflow_bytes, last_t);
        last_t = t(nev);
        ind = ind + nbbytes;
        overflow_bytes = 0;
      elseif (data(ind) == overflow_check)
        overflow_bytes = 0;
        ind = ind + 1;
      else
        overflow_bytes = overflow_bytes + uint64(bitand(data(ind),overflow_bytes_check));
        ind = ind + 1;
      end
    end
    events.t = t(1:nev);
    events.x = x(1:nev);
    events.y = y(1:nev);
    events.is_threshold_crossing = is_threshold_crossing(1:nev);
    events.polarity = polarity(1:nev);

  case 'color'
    header.width = fread(fid,1,'*uint16');
    header.height = fread(fid,1,'*uint16');
    data = fread(fid,Inf,'*uint8');
    fclose(fid);

    nev_max = floor(numel(data)/5);
    t = zeros(nev_max,1,'uint64');
    x = zeros(nev_max,1,'uint16');
    y = zeros(nev_max,1,'uint16');
    r = zeros(nev_max,1,'uint8');
    g = zeros(nev_max,1,'uint8');
    b = zeros(nev_max,1,'uint8');
    overflow_check = 254;
    overflow_bytes_check = 1;
    nbbytes = 8;

    nev = 0;
    overflow_bytes = 0;
    last_t = 0;
    ind = 1;
    while ind<numel(data)
      if(data(ind)<overflow_check)
        nev = nev + 1;
        [t(nev), x(nev), y(nev), r(nev), g(nev), b(nev)] = ...
          bytes_color_to_event(data(ind+(0:(nbbytes-1))), overflow_bytes, last_t);
        last_t = t(nev);
        ind = ind + nbbytes;
        overflow_bytes = 0;
      elseif (data(ind) == overflow_check)
        overflow_bytes = 0;
        ind = ind + 1;
      else
        overflow_bytes = overflow_bytes + uint64(bitand(data(ind),overflow_bytes_check));
        ind = ind + 1;
      end
    end
    events.t = t(1:nev);
    events.x = x(1:nev);
    events.y = y(1:nev);
    events.r = r(1:nev);
    events.g = g(1:nev);
    events.b = b(1:nev);

  otherwise
    fclose(fid);
    fprintf('Version (Major, Minor, Patch) : (%d, %d, %d)\n', major_version, ...
      minor_version, patch_version);
    if isempty(header.type)
      error('Stream type unknown');
    else
      error('The Matlab state machine for this stream type is not coded yet.');
    end
end

end

function [t, x, y, p] = bytes_dvs_to_event(bytes, overflow_bytes, last_t)
  p = bitget(bytes(1), 1);
  t = uint64(bitshift(bytes(1),-1))+127*overflow_bytes+last_t;
  x = uint16(bytes(3))*256+uint16(bytes(2));
  y = uint16(bytes(5))*256+uint16(bytes(4));
end

function [t, x, y, th, p] = bytes_atis_to_event(bytes, overflow_bytes, last_t)
  th = bitget(bytes(1), 1);
  p = bitget(bytes(1), 2);
  t = uint64(bitshift(bytes(1),-2))+63*overflow_bytes+last_t;
  x = uint16(bytes(3))*256+uint16(bytes(2));
  y = uint16(bytes(5))*256+uint16(bytes(4));
end

function [t, x, y, r, g, b] = bytes_color_to_event(bytes, overflow_bytes, last_t)
  t = uint64(bytes(1))+254*overflow_bytes+last_t;
  x = uint16(bytes(3))*256+uint16(bytes(2));
  y = uint16(bytes(5))*256+uint16(bytes(4));
  r = bytes(6);
  g = bytes(7);
  b = bytes(8);
end
