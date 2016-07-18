classdef ALearner < Learner
    %ALEARNER Summary of this class goes here
    %   Detailed explanation goes here
    
    properties
        numMods = 0;
        modDims = [];
        modBeginEnd = [];
        drivenIdx = [];
        feedbackIdx = [];
        conv = 0;
        maxSteps = 10000;
        maxDX = 1e-4;
        inp = [];
    end
    
    methods
        function l = ALearner(modDims, spec)
            inpDim = sum(modDims);
            outDim = inpDim;
            l = l@Learner(inpDim, outDim, spec);
            
            l.modDims = modDims;
            l.numMods = length(l.modDims);
            l.modBeginEnd = zeros(l.numMods,2);
            tmp = [0 l.modDims];
            for m=2:l.numMods+1
                l.modBeginEnd(m-1,1) = sum(tmp(1:m-1))+1;
                l.modBeginEnd(m-1,2) = sum(tmp(1:m));
            end
            l.inp = zeros(1,l.inpDim);
        end
        
        function setDrivenMods(l, mods)
            %collect the clamped input indices
            l.drivenIdx = [];
            for m=1:length(mods)
                l.drivenIdx = [l.drivenIdx l.modBeginEnd(mods(m),1):l.modBeginEnd(mods(m),2)];
            end
            %get the components running in the feedback loop
            l.feedbackIdx = setdiff(1:l.inpDim, l.drivenIdx);
        end
        
        function setDrivenIdx(l, idx)
            l.drivenIdx = idx;
            l.feedbackIdx = setdiff(1:l.inpDim, l.drivenIdx);
        end
        
        function [Xhat,Xvar] = apply(l, X)
            if iscell(X)
                Xhat = cell(length(X),1);
                Xvar = cell(length(X),1);
                for i=1:length(X)
                    [Xhat{i},Xvar{i}] = apply@ALearner(l, X{i});
                end
            else
                if ~isempty(l.inpOffset)
                    X = range2norm(X, l.inpRange, l.inpOffset);
                end
                Xhat = zeros(size(X));
                Xvar = zeros(size(X,1),1);
                if l.conv
                    p = randperm(size(X,1));
                    warned = 0;
                    for i=1:size(X,1)
                        l.inp = X(p(i),:);
                        steps = l.converge();
                        if isempty(l.outOffset)
                            Xhat(p(i),:) = l.out;
                        else
                            Xhat(p(i),:) = norm2range(l.out, l.inpRange, l.inpOffset);
                        end
                        Xvar(i) = l.outvar;
                        if ~warned && steps >= l.maxSteps
                            disp('WARNING: ALearner did not converge');
                            warned = 1;
                        end
                    end
                else
                    for i=1:size(X,1)
                        %feedback and clamp inputs
                        l.inp = l.out;
                        l.inp(l.drivenIdx) = X(i,l.drivenIdx);
                        %update the model
                        l.updateOut();
                        if isempty(l.outOffset)
                            Xhat(i,:) = l.out;
                        else
                            Xhat(i,:) = norm2range(l.out, l.inpRange, l.inpOffset);
                            
                        end
                        Xvar(i) = l.outvar;
                    end
                end
            end
        end
        
        %implement this function in subclasses
        function updateOut(l) %maybe with inp argument for reusability in training fct impls (and faster with matrix input)
        end
        
        function steps = converge(l)
            clampedinp = l.inp;
            dx = 99999.9;
            steps = 0;
            while ((dx > l.maxDX) || (steps < 2))  && steps < l.maxSteps
                %feedback and clamp inputs
                l.inp = l.out;
                l.inp(l.drivenIdx) = clampedinp(l.drivenIdx);
                %update the model
                l.updateOut();
                %monitor change of feedback variables
                dx = sqrt(sum((l.inp(l.feedbackIdx) - l.out(l.feedbackIdx)).^2));
                steps = steps + 1;
            end
            %disp(['Convergence took ' num2str(steps) ' Steps']);
        end
        
    end
    
end

