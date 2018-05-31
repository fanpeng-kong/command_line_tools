%READ_EVENT_STREAM Help file for read_event_stream MEX-file
%   read_event_stream.cpp - Read Event Stream files
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
