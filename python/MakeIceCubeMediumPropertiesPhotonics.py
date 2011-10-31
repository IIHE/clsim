from icecube import icetray, dataclasses
from icecube.clsim import I3CLSimMediumProperties, \
                          I3CLSimRandomValueMixed, \
                          I3CLSimRandomValueHenyeyGreenstein, \
                          I3CLSimRandomValueSimplifiedLiu, \
                          I3CLSimWlenDependentValueFromTable

from I3Tray import I3Units

import numpy, math
from os.path import expandvars


def MakeIceCubeMediumPropertiesPhotonics(tableFile,
                                         detectorCenterDepth = 1948.07*I3Units.m):
    ### read ice information from a photonics-compatible table
    
    file = open(tableFile, 'r')
    rawtable = file.readlines()    
    file.close()
    del file
    
    # get rid of comments and split lines at whitespace
    parsedtable = [line.split() for line in rawtable if line.lstrip()[0] != "#"]

    nlayer_lines = [line for line in parsedtable if line[0].upper()=="NLAYER"]
    nwvl_lines =   [line for line in parsedtable if line[0].upper()=="NWVL"]
    
    if len(nlayer_lines) == 0:
        raise RuntimeError("There is no \"NLAYER\" entry in your ice table!")
    if len(nlayer_lines) > 1:
        raise RuntimeError("There is more than one \"NLAYER\" entry in your ice table!")
    if len(nwvl_lines) == 0:
        raise RuntimeError("There is no \"NWVL\" entry in your ice table!")
    if len(nwvl_lines) > 1:
        raise RuntimeError("There is more than one \"NWVL\" entry in your ice table!")
    
    nLayers = int(nlayer_lines[0][1])
    nWavelengths = int(nwvl_lines[0][1])
    startWavelength = float(nwvl_lines[0][2])*I3Units.nanometer
    stepWavelength = float(nwvl_lines[0][3])*I3Units.nanometer

    startWavelength += stepWavelength/2.
    
    # make a list of wavelengths
    wavelengths = numpy.linspace(start=startWavelength, stop=startWavelength+float(nWavelengths)*stepWavelength, num=nWavelengths, endpoint=False)
    
    print "The table file {} has {} layers and {} wavelengths, starting from {}ns in {}ns steps".format(tableFile, nLayers, nWavelengths, startWavelength/I3Units.nanometer, stepWavelength/I3Units.nanometer)
    
    
    # replace parsedtable with a version without NLAYER and NWVL entries
    parsedtable = [line for line in parsedtable if ((line[0].upper()!="NLAYER") and (line[0].upper()!="NWVL"))]
    
    if len(parsedtable) != nLayers*6:
        raise RuntimeError("Expected {}*6={} lines [not counting NLAYER and NWVL] in the icetable file, found {}".format(nLayers, nLayers*6, len(parsedtable)))
    
    if parsedtable[0][0].upper() != "LAYER":
        raise RuntimeError("Layer definitions should start with the LAYER keyword (reading {})".format(tableFile))
    
    # parse the lines
    layers = []
    
    layerdict = dict()
    for line in parsedtable:
        keyword = line[0].upper()

        if keyword == "LAYER":
            if len(layerdict) != 0:
                layers.append(layerdict)
            
            #re-set layerdict
            layerdict = dict()
        else:
            # some other keyword
            if keyword in layerdict:
                raise RuntimeError("Keyword {} is used twice for one layer (reading {})".format(keyword, tableFile))

        # optional units
        unit = 1.                                       # no units by default
        if keyword is "LAYER": unit = I3Units.meter     # coordinates
        if keyword is "ABS": unit = 1./I3Units.meter    # absorption coefficient
        if keyword is "SCAT": unit = 1./I3Units.meter   # scattering coefficient

        layerdict[keyword] = [float(number)*unit for number in line[1:]]

    # last layer
    if len(layerdict) != 0:
        layers.append(layerdict)        

    if len(layers) < 1:
        raise RuntimeError("At least one layers is requried (reading {})".format(tableFile))

    layerHeight = abs(layers[0]['LAYER'][1] - layers[0]['LAYER'][0])
    print "layer height is {}m".format(layerHeight/I3Units.m)
    
    # sort layers into dict by bottom z coordinate and check some assumptions
    layersByZ = dict()
    for layer in layers:
        layerBottom = layer['LAYER'][0]
        layerTop = layer['LAYER'][1]
        
        if layerBottom > layerTop:
            print "a layer is upside down. compensating. (reading {})".format(tableFile)
            dummy = layerBottom
            layerBottom = layerTop
            layerTop = dummy
        
        # check layer height
        if abs((layerTop-layerBottom) - layerHeight) > 0.0001:
            raise RuntimeError("Differing layer heights while reading {}. Expected {}m, got {}m.".format(tableFile, layerHeight, layerTop-layerBottom))
        
        layersByZ[layerBottom] = layer
    
    
    layers = []
    endZ = None
    for dummy, layer in sorted(layersByZ.iteritems()):
        startZ = layer['LAYER'][0]
        if (endZ is not None) and (abs(endZ-startZ) > 0.0001):
            raise RuntimeError("Your layers have holes. previous layer ends at {}m, next one starts at {}m".format(endZ/I3Units.m, startZ/I3Units.m))
        endZ = layer['LAYER'][1]
        layers.append(layer)
    
    # check even more assumptions
    meanCos=None
    for layer in layers:
        if meanCos is None: meanCos = layer['COS'][0]
        
        for cosVal in layer['COS']:
            if abs(cosVal-meanCos) > 0.0001:
                raise RuntimeError("only a consant mean cosing is supported by clsim (expected {}, got {})".format(meanCos, cosVal))
        
        if len(layer['COS']) != len(wavelengths):
            raise RuntimeError("Expected {} COS values, got {}".format(len(wavelengths), len(layer['COS'])))
        if len(layer['ABS']) != len(wavelengths):
            raise RuntimeError("Expected {} ABS values, got {}".format(len(wavelengths), len(layer['ABS'])))
        if len(layer['SCAT']) != len(wavelengths):
            raise RuntimeError("Expected {} SCAT values, got {}".format(len(wavelengths), len(layer['SCAT'])))
        if len(layer['N_GROUP']) != len(wavelengths):
            raise RuntimeError("Expected {} N_GROUP values, got {}".format(len(wavelengths), len(layer['N_GROUP'])))
        if len(layer['N_PHASE']) != len(wavelengths):
            raise RuntimeError("Expected {} N_PHASE values, got {}".format(len(wavelengths), len(layer['N_PHASE'])))

        for i in range(len(wavelengths)):
            if abs(layer['N_GROUP'][i]-layers[0]['N_GROUP'][i]) > 0.0001:
                raise RuntimeError("N_GROUP may not be different for different layers in this version of clsim!")

            if abs(layer['N_PHASE'][i]-layers[0]['N_PHASE'][i]) > 0.0001:
                raise RuntimeError("N_PHASE may not be different for different layers in this version of clsim!")
        
        #print "start", layer['LAYER'][0], "end", layer['LAYER'][1]
    
    
    ##### start making the medium property object
    
    m = I3CLSimMediumProperties(mediumDensity=0.9216*I3Units.g/I3Units.cm3,
                                layersNum=len(layers),
                                layersZStart=layers[0]['LAYER'][0],
                                layersHeight=layerHeight,
                                rockZCoordinate=-870.*I3Units.m,
                                # TODO: inbetween: from 1740 upwards: less dense ice (factor 0.825)
                                airZCoordinate=1940.*I3Units.m)
    
    
    iceCubeScatModel = I3CLSimRandomValueHenyeyGreenstein(meanCosine=meanCos)
    m.SetScatteringCosAngleDistribution(iceCubeScatModel)
    
    phaseRefIndex = I3CLSimWlenDependentValueFromTable(startWavelength, stepWavelength, layers[0]['N_PHASE'])
    groupRefIndex = I3CLSimWlenDependentValueFromTable(startWavelength, stepWavelength, layers[0]['N_GROUP'])

    for i in range(len(layers)):
        #print "layer {0}: depth at bottom is {1} (z_bottom={2}), b_400={3}".format(i, depthAtBottomOfLayer[i], layerZStart[i], b_400[i])
        
        m.SetPhaseRefractiveIndex(i, phaseRefIndex)
        m.SetGroupRefractiveIndexOverride(i, groupRefIndex)
        
        absLenTable = [1./absCoeff for absCoeff in layers[i]['ABS']]
        absLen = I3CLSimWlenDependentValueFromTable(startWavelength, stepWavelength, absLenTable)
        m.SetAbsorptionLength(i, absLen)

        scatLenTable = [(1./scatCoeff)*(1.-meanCos) for scatCoeff in layers[i]['SCAT']]
        scatLen = I3CLSimWlenDependentValueFromTable(startWavelength, stepWavelength, scatLenTable)
        m.SetScatteringLength(i, scatLen)

    return m

