classdef Learner < handle
    %LEARNER Interface class for function approximator models.
    %   Detailed explanation goes here
    
    properties
        inpDim = 0; %dimension of input
        outDim = 0; %dimension of output
        out = [];
        outvar=NaN;
        
        inpOffset = [];
        inpRange = [];
        outOffset = [];
        outRange = [];
    end
    
    methods
        function l = Learner(inpDim, outDim, spec)
            l.inpDim = inpDim;
            l.outDim = outDim;
            l.out = zeros(1,l.outDim);
            
            if nargin > 2
                for s=1:size(spec,1)
                    if ~strcmp(spec{s,1}, 'class')
                        try
                            l.(spec{s,1}) = spec{s,2};
                        catch me
                            disp(me.message);
                        end
                    end
                end
            end
        end
        
        function [X Y] = normalizeIO(l, X, Y)
            [X l.inpOffset l.inpRange] = normalize(X);
            if exist('Y', 'var')
                [Y l.outOffset l.outRange] = normalize(Y);
            end
        end
        
        function init(l, X)
        end
        
        % expect X and Y to be either matrices or cell-arrays of matrices
        function train(l, X, Y)
        end
        
        function Y = apply(l, X)
            
        end
        
        function new = copy(this)
            new = feval(class(this));
            p = properties(this);
            for i = 1:length(p)
                if( isa(this.(p{i}), 'handle'))
                    new.(p{i}) = this.(p{i}).copy();
                elseif isa(this.(p{i}),'cell')
                    new.(p{i}) = deepCopyCellArray(numel(this.(p{i})));
                else
                    new.(p{i}) = this.(p{i});
                end
            end
        end
    end
    
end

