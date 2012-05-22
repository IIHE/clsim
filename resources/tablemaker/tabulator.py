#!/usr/bin/env python

from icecube.icetray import I3Units, I3Module, traysegment
from icecube.dataclasses import I3Position, I3Particle, I3MCTree, I3Direction, I3Constants
from icecube.phys_services import I3Calculator, I3GSLRandomService
from icecube.clsim import I3Photon, I3CLSimTabulator, GetIceCubeDOMAcceptance, GetIceCubeDOMAngularSensitivity
import numpy, math
from icecube.photospline import numpy_extensions # meshgrid_nd
	
class PhotoTable(object):
	
	def __init__(self, binedges, values, weights, n_photons=0):
		self.binedges = binedges
		self.values = values
		self.weights = weights
		self.bincenters = [(edges[1:]+edges[:-1])/2. for edges in self.binedges]
		self.n_photons = n_photons
		
	def __iadd__(self, other):
		if not isinstance(other, self.__class__):
			raise TypeError, "Can only add another %s" % (self.__class__.__name__)
		self.values += other.values
		self.weights += other.weights
		self.n_photons += other.n_photons
		
		return self
	
	def __idiv__(self, num):
		return self.__imul__(1./num)
		
	def __imul__(self, num):
		self.values *= num
		self.weights *= num*num
		
		return self
		
	def normalize(self):
		self /= self.n_photons
		
	def save(self, fname):
		import pyfits
		
		data = pyfits.PrimaryHDU(self.values)
		data.header.update('TYPE', 'Photon detection probability table')
		
		data.header.update('NPHOTONS', self.n_photons)
		
		hdulist = pyfits.HDUList([data])
		
		if self.weights is not None:
			errors = pyfits.ImageHDU(self.weights, name='ERRORS')
			hdulist.append(errors)
		
		for i in xrange(self.values.ndim):
			edgehdu = pyfits.ImageHDU(self.binedges[i],name='EDGES%d' % i)
			hdulist.append(edgehdu)
			
		hdulist.writeto(fname)
		
	@classmethod
	def load(cls, fname):
		import pyfits
		
		hdulist = pyfits.open(fname)
		data = hdulist[0]
		values = data.data
		binedges = []
		for i in xrange(values.ndim):
			binedges.append(hdulist['EDGES%d' % i].data)
		
		try:
			weights = hdulist['ERRORS'].data
		except KeyError:
			weights = None
			
		n_photons = data.header['NPHOTONS']
			
		return cls(binedges, values, weights, n_photons)
	
class Tabulator(object):
	_dtype = numpy.float32
	
	
	def SetBins(self, binedges, step_length=1.*I3Units.m):
		self.binedges = binedges
		self.values = numpy.zeros(tuple([len(v)-1 for v in self.binedges]), dtype=self._dtype)
		self.weights = numpy.zeros(self.values.shape, dtype=self._dtype)
		
		self._step_length = step_length
		
	def SetEfficiencies(self, wavelengthAcceptance=GetIceCubeDOMAcceptance(), angularAcceptance=GetIceCubeDOMAngularSensitivity(), domRadius=0.16510*I3Units.m):
		self._angularEfficiency = angularAcceptance
		domArea = numpy.pi*domRadius**2
		
		self._effectiveArea = lambda wvl: domArea*wavelengthAcceptance.GetValue(wvl)
	
	def SetRandomService(self, rng):
		self.rng = rng
	
	def GetValues(self):
		return self.values, self.weights	
		
	# def __init__(self, nbins=(200, 36, 100, 105)):
	# 	self.binedges = [
	# 	    numpy.linspace(0, numpy.sqrt(580), nbins[0]+1)**2,
	# 	    numpy.linspace(0, 180, nbins[1]+1),
	# 	    numpy.linspace(-1, 1, nbins[2]+1),
	# 	    numpy.linspace(0, numpy.sqrt(7e3), nbins[3]+1)**2,
	# 	]
	# 	self._dtype = numpy.float32
	# 	self.values = numpy.zeros(tuple([len(v)-1 for v in self.binedges]), dtype=self._dtype)
	# 	self.weights = numpy.zeros(self.values.shape, dtype=self._dtype)
	# 	self.n_photons = 0
	# 	self._step_length = 1*I3Units.m
	# 	self._angularEfficiency = GetIceCubeDOMAngularSensitivity()
	# 	
	# 	# The efficiency returned by I3CLSimMakePhotons() is the effective
	# 	# area of a standard DOM as a function of wavelength, divided by the
	# 	# projected area of the DOM to make it unitless, and multiplied by
	# 	# 1.35 to ensure that detection probabilities for high-QE DOMs are
	# 	# always <= 1. When making hits from photon-DOM collisions these factors
	# 	# drop out, but we have to correct for them in volume-density mode. 
	# 	# efficiency = GetIceCubeDOMAcceptance(efficiency=1.35*0.75*1.01)
	# 	efficiency = GetIceCubeDOMAcceptance()
	# 	domRadius = 0.16510*I3Units.m
	# 	domArea = numpy.pi*domRadius**2
	# 	
	# 	self._effectiveArea = lambda wvl: domArea*efficiency.GetValue(wvl)
		
	def save(self, fname):
		# normalize to a photon flux, but not the number of photons (for later stacking)
		area = self._getBinAreas()
		print "Total weights:", self.n_photons
		PhotoTable(self.binedges, self.values/area, self.weights/(area*area), self.n_photons).save(fname)
	
	def _getBinAreas(self):
		
		near = numpy.meshgrid_nd(*[b[:-1].astype(self._dtype) for b in self.binedges[:-1]], lex_order=True)
		far = numpy.meshgrid_nd(*[b[1:].astype(self._dtype) for b in self.binedges[:-1]], lex_order=True)
		
		# The effective area of the bin is its volume divided by
		# the sampling frequency. 
		# NB: since we're condensing onto a half-sphere, the azimuthal extent of each bin is doubled.
		area = ((far[0]**3-near[0]**3)/3.)*(2*(far[1]-near[1])*I3Units.degree)*(far[2]-near[2])
		print 'Total volume:', area.sum()
		area = area.reshape(area.shape + (1,))/self._dtype(self._step_length)
		return area
		
	@staticmethod
	def GetBinVolumes(binedges, dtype=numpy.float32):
		
		near = numpy.meshgrid_nd(*[b[:-1].astype(dtype) for b in binedges[:-1]], lex_order=True)
		far = numpy.meshgrid_nd(*[b[1:].astype(dtype) for b in binedges[:-1]], lex_order=True)
		
		# The effective area of the bin is its volume divided by
		# the sampling frequency. 
		# NB: since we're condensing onto a half-sphere, the azimuthal extent of each bin is doubled.
		volumes = ((far[0]**3-near[0]**3)/3.)*(2*(far[1]-near[1])*I3Units.degree)*(far[2]-near[2])
		volumes = volumes.reshape(volumes.shape + (1,))
		return volumes
	
	def Normalize(self):
		
		fluxconst = self._getBinAreas()/self._step_length
		
		# convert each weight (in units of photons * m^2) to a probability
		self.values /= fluxconst
		self.weights /= fluxconst*fluxconst
	
	def _getDirection(self, pos1, pos2):
		d = I3Direction(pos2.x-pos1.x, pos2.y-pos1.y, pos2.z-pos1.z)
		return numpy.array([d.x, d.y, d.z])
		
	def _getPerpendicularDirection(self, d):
		perpz = math.hypot(d.x, d.y)
		if perpz > 0:
			return numpy.array([-d.x*d.z/perpz, -d.y*d.z/perpz, perpz])
		else:
			return numpy.array([1., 0., 0.])

	def _getBinIndices(self, source, pos, t):
		"""
		This is the only function that needs to be significantly modified
		to support different table geometries.
		
		NB: the currently implements sperical, source-centered coordinates only!
		
		TODO: implement detector-centered (degenerate) cylindrical coords
		"""
		p0 = numpy.array(source.pos)
		p1 = numpy.array(pos)
		
		dt = I3Calculator.time_residual(source, pos, t, I3Constants.n_ice_group, I3Constants.n_ice_phase)
		
		source_dir = numpy.array([source.dir.x, source.dir.y, source.dir.z])
		perp_dir = self._getPerpendicularDirection(source.dir)
		
		norm = lambda arr: numpy.sqrt((arr**2).sum())
		
		displacement = p1-p0
		r = norm(displacement)
		l = numpy.dot(displacement, source_dir)
		rho = displacement - l*source_dir
		if norm(rho) > 0:
			azimuth = numpy.arccos(numpy.dot(rho, perp_dir)/norm(rho))/I3Units.degree
		else:
			azimuth = 0
		if r > 0:
			cosZen = l/r
		else:
			cosZen = 1
		idx = [numpy.searchsorted(edges, v) for v, edges in zip((r, azimuth, cosZen, dt), self.binedges)]
		for j, i in enumerate(idx):
			if i > len(self.binedges[j])-2:
				idx[j] = len(self.binedges[j])-2
		return tuple(idx)

	def RecordPhoton(self, source, photon):
		"""
		:param nphotons: number of photons to add to the total tally
		after recording. Set this to zero if you're going to do your own counting.
		"""
		# position of photon at each scatter
		positions = photon.positionList
		# path length in units of absorption length at each scatter
		abs_lengths = [photon.GetDistanceInAbsorptionLengthsAtPositionListEntry(i) for i in xrange(len(positions))]
		
		# The various weights are constant for different bits of the photon track.
		# Constant for a given photon:
		# - Photon weight: 1 / wavelength bias
		# - DOM effective area (as a function of wavelength)
		# Constant for paths between scatters:
		# - relative angular efficiency (as a function of impact angle)
		# Varies along the track:
		# - survival probability (exp(-path/absorption length))
		# The units of the combined weight are m^2 / photon
		wlen_weight = self._effectiveArea(photon.wavelength)*photon.weight
		
		t = photon.startTime
		
		for start, start_lengths, stop, stop_lengths in zip(positions[:-1], abs_lengths[:-1], positions[1:], abs_lengths[1:]):
			if start is None or stop is None:
				continue
			pdir = self._getDirection(start, stop)
			# XXX HACK: the cosine of the impact angle with the
			# DOM is the same as the z-component of the photon
			# direction if the DOM is pointed straight down.
			# This can be modified for detectors with other values
			# of pi.
			impact = pdir[2]
			distance = start.calc_distance(stop)
			# segment length in units of the local absorption length
			abs_distance = stop_lengths-start_lengths 
			
			impact_weight = wlen_weight*self._angularEfficiency.GetValue(impact)
			
			# FIXME: this samples at fixed distances. Randomize instead?
			nsamples = int(numpy.floor(distance/self._step_length))
			nsamples += int(self.rng.uniform(0,1) < (distance/self._step_length - nsamples))
			for i in xrange(nsamples):
				d = distance*self.rng.uniform(0,1)
			# for d in numpy.arange(0, distance, self._step_length):
				# Which bin did we land in?
				pos = I3Position(*(numpy.array(start) + d*pdir))
				indices = self._getBinIndices(source, pos, t + d/photon.groupVelocity)
				# We've run off the edge of the recording volume. Bail.
				if indices[0] == len(self.binedges[0]) or indices[3] == len(self.binedges[3]):
					break
				d_abs = start_lengths + (d/distance)*abs_distance
				weight = impact_weight*numpy.exp(-d_abs)
				
				self.values[indices] += weight
				self.weights[indices] += weight*weight
			
			t += distance/photon.groupVelocity

# tabulator = Tabulator
tabulator = I3CLSimTabulator
		
class I3TabulatorModule(I3Module, tabulator):
	def __init__(self, ctx):
		I3Module.__init__(self, ctx)
		tabulator.__init__(self)
		
		self.AddParameter("Filename", "Output filename", "foo.fits")
		self.AddParameter("Photons", "Name of I3PhotonSeriesMap in the frame", "I3PhotonSeriesMap")
		self.AddParameter("Statistics", "Name of the CLSimStatistics object in the frame", "")
		self.AddParameter("Source", "Name of the source I3Particle in the frame", "")
		self.AddOutBox("OutBox")
		
	def Configure(self):
		
		self.fname = self.GetParameter("Filename")
		self.photons = self.GetParameter("Photons")
		self.stats = self.GetParameter("Statistics")
		self.source = self.GetParameter("Source")
		
		self.n_photons = 0
		
		nbins=(200, 36, 100, 105)
		self.binedges = [
		    numpy.linspace(0, numpy.sqrt(580), nbins[0]+1)**2,
		    numpy.linspace(0, 180, nbins[1]+1),
		    numpy.linspace(-1, 1, nbins[2]+1),
		    numpy.linspace(0, numpy.sqrt(7e3), nbins[3]+1)**2,
		]
		
		self.domRadius = 0.16510*I3Units.m
		self.stepLength = 1*I3Units.m
		
		self.SetBins(self.binedges, self.stepLength)
		self.SetEfficiencies(GetIceCubeDOMAcceptance(), GetIceCubeDOMAngularSensitivity(), self.domRadius)
		self.SetRandomService(I3GSLRandomService(12345))
		
	def DAQ(self, frame):
		source = frame[self.source]
		photonmap = frame[self.photons]
		
		for photons in photonmap.itervalues():
			# wtot = sum([photon.weight for photon in photons])
			wsum = 0
			for photon in photons:
				self.RecordPhoton(source, photon)
				# wsum += photon.weight
				# if int(wsum/wtot)*100 % 10 == 0:
				# 	print 'Tabulated weights %f/%f' % (wsum,wtot)
		
		# Each I3Photon can only carry a fixed number of intermediate
		# steps, so there may be more than one I3Photon for each generated
		# photon. Since the generation spectrum is biased, we want to
		# normalize to the sum of weights, not the number of photons.
		stats = frame[self.stats]
		print '%f photons at DOMs (total weight %f), %f generated (total weight %f)' % (
		    stats.GetTotalNumberOfPhotonsAtDOMs(), stats.GetTotalSumOfWeightsPhotonsAtDOMs(),
		    stats.GetTotalNumberOfPhotonsGenerated(), stats.GetTotalSumOfWeightsPhotonsGenerated())
		self.n_photons += stats.GetTotalSumOfWeightsPhotonsAtDOMs()
		
		self.PushFrame(frame)
		
	def Finish(self):
		# pass
		
		# self.Normalize()
		values, weights = self.GetValues()
		# print values, weights
		print values.shape, weights.shape
		
		domArea = numpy.pi*self.domRadius**2
		
		norm = Tabulator.GetBinVolumes(self.binedges)/(domArea*self.stepLength)
		print values.max(), weights.max()
		table = PhotoTable(self.binedges, values/norm, weights/(norm**2), self.n_photons)
		table.save(self.fname)

class MakeParticle(I3Module):
    def __init__(self, context):
        I3Module.__init__(self, context)
        self.AddParameter("I3RandomService", "the service", None)
        self.AddParameter("Type", "", I3Particle.ParticleType.EMinus)
        self.AddParameter("Energy", "", 1.*I3Units.GeV)
        self.AddParameter("Zenith", "", 90.*I3Units.degree)
        self.AddParameter("Azimuth", "", 0.*I3Units.degree)
        self.AddParameter("ZCoordinate", "", 0. * I3Units.m)
        self.AddParameter("NEvents", "", 100)

        self.AddOutBox("OutBox")        

    def Configure(self):
        self.rs = self.GetParameter("I3RandomService")
        self.particleType = self.GetParameter("Type")
        self.energy = self.GetParameter("Energy")
        self.zenith = self.GetParameter("Zenith")
        self.azimuth = self.GetParameter("Azimuth")
        self.zCoordinate = self.GetParameter("ZCoordinate")
        self.nEvents = self.GetParameter("NEvents")
        
        self.emittedEvents=0

    def DAQ(self, frame):
        daughter = I3Particle()
        daughter.type = self.particleType
        daughter.energy = self.energy
        daughter.pos = I3Position(0., 0., self.zCoordinate)
        daughter.dir = I3Direction(self.zenith,self.azimuth)
        daughter.time = 0.
        daughter.location_type = I3Particle.LocationType.InIce

        primary = I3Particle()
        primary.type = I3Particle.ParticleType.NuE
        primary.energy = self.energy
        primary.pos = I3Position(0., 0., self.zCoordinate)
        primary.dir = I3Direction(self.zenith,self.azimuth)
        primary.time = 0.
        primary.location_type = I3Particle.LocationType.Anywhere

        mctree = I3MCTree()
        mctree.add_primary(primary)
        mctree.append_child(primary,daughter)
    
        # clsim likes I3MCTrees
        frame["I3MCTree"] = mctree

        # the table-making module prefers plain I3Particles
        frame["Source"] = daughter

        self.emittedEvents += 1
        
        self.PushFrame(frame)
        
        print self.emittedEvents
        if self.emittedEvents >= self.nEvents:
            self.RequestSuspension()

		
@traysegment
def PhotonGenerator(tray, name, Seed=12345, NEvents=100):
	from icecube import icetray, dataclasses, dataio, phys_services, sim_services, clsim
	from os.path import expandvars
	
	# a random number generator
	randomService = phys_services.I3SPRNGRandomService(
	    seed = Seed,
	    nstreams = 10000,
	    streamnum = 0)
	    
	tray.AddModule("I3InfiniteSource",name+"streams",
	               Stream=icetray.I3Frame.DAQ)

	tray.AddModule("I3MCEventHeaderGenerator",name+"gen_header",
	               Year=2009,
	               DAQTime=158100000000000000,
	               RunNumber=1,
	               EventID=1,
	               IncrementEventID=True)

	# empty GCD
	tray.AddService("I3EmptyStreamsFactory", name+"empty_streams",
	    InstallCalibration=True,
	    InstallGeometry=True,
	    InstallStatus=True)
	tray.AddModule("I3MetaSynth", name+"synth")


	tray.AddModule(MakeParticle, name+"MakeParticle",
	    Zenith = 90.*I3Units.deg,
	    ZCoordinate = 0.*I3Units.m,
	    # Energy = 0.01*I3Units.GeV, # the longitudinal profile parameterization from PPC breaks down at this energy (the cascade will be point-like). The angular parameteriztion will still work okay. This might be exactly what we want for table-making (it should become an option to the PPC parameterization eventually).
	    NEvents = NEvents)


	tray.AddSegment(clsim.I3CLSimMakePhotons, name+"makeCLSimPhotons",
	    PhotonSeriesName = "PropagatedPhotons",
	    MCTreeName = "I3MCTree",
	    MMCTrackListName = None,    # do NOT use MMC
	    ParallelEvents = 1,         # only work at one event at a time (it'll take long enough)
	    RandomService = randomService,
	    # UnWeightedPhotons=True,
	    UseGPUs=False,              # table-making is not a workload particularly suited to GPUs
	    UseCPUs=True,               # it should work fine on CPUs, though
	    UnshadowedFraction=1.0,     # no cable shadow
	    DOMOversizeFactor=1.0,      # no oversizing (there are no DOMs, so this is pointless anyway)
	    StopDetectedPhotons=False,  # do not stop photons on detection (also somewhat pointless without DOMs)
	    PhotonHistoryEntries=10000, # record all photon paths
	    DoNotParallelize=True,      # no multithreading
	    OverrideApproximateNumberOfWorkItems=1, # if you *would* use multi-threading, this would be the maximum number of jobs to run in parallel (OpenCL is free to split them)
	    ExtraArgumentsToI3CLSimModule=dict(SaveAllPhotons=True,                 # save all photons, regardless of them hitting anything
	                                       SaveAllPhotonsPrescale=1.,           # do not prescale the generated photons
	                                       StatisticsName="I3CLSimStatistics",  # save a statistics object (contains the initial number of photons)
	                                       FixedNumberOfAbsorptionLengths=46.,  # this is approx. the number used by photonics (it uses -ln(1e-20))
	                                       LimitWorkgroupSize=1,                # this effectively disables all parallelism (there should be only one OpenCL worker thread)
	                                                                            #  it also should save LOTS of memory
	                                      ),
	    # IceModelLocation=expandvars("$I3_SRC/clsim/resources/ice/photonics_wham/Ice_table.wham.i3coords.cos094.11jul2011.txt"),
	    IceModelLocation=expandvars("$I3_SRC/clsim/resources/ice/photonics_spice_1/Ice_table.spice.i3coords.cos080.10feb2010.txt"),
	    #IceModelLocation=expandvars("$I3_SRC/clsim/resources/ice/spice_mie"),
	    )

def potemkin_photon():
	ph = I3Photon()
	ph.numScattered = 1
	ph.groupVelocity = I3Constants.c/I3Constants.n_ice_group
	ph.startPos = I3Position(0,0,0)
	ph.startTime = 0.
	ph.pos = I3Position(100., 100., 100.,)
	ph.AppendToIntermediatePositionList(I3Position(100., 0., 0.,), 1)
	ph.distanceInAbsorptionLengths = 2
	ph.wavelength = 450.
	ph.weight = 1.
	
	return ph

def potemkin_source():
	p = I3Particle()
	p.pos = I3Position(0,0,0)
	p.dir = I3Direction(0,0,1)
	p.time = 0
	p.shape = p.Cascade
	p.energy = 1e3
	
	return p

def test_tray():
	from I3Tray import I3Tray
	from icecube import dataio, phys_services
	from icecube.clsim import I3PhotonSeries, I3PhotonSeriesMap, I3CLSimEventStatistics
	from icecube.icetray import OMKey, I3Frame
	
	tray = I3Tray()
	
	tray.AddModule('I3InfiniteSource', 'maw')
	
	def inserty(frame):
		source = potemkin_source()
		psm = I3PhotonSeriesMap()
		psm[OMKey(0,0)] = I3PhotonSeries([potemkin_photon()])
		
		stats = I3CLSimEventStatistics()
		stats.AddNumPhotonsGeneratedWithWeights(numPhotons=1, weightsForPhotons=1, majorID=0, minorID=0)
		
		frame['Source'] = source
		frame['Photons'] = psm
		frame['Statistics'] = stats
		
	tray.AddModule(inserty, 'inserty', Streams=[I3Frame.DAQ])
	
	import os
	if os.path.exists('potemkin.fits'):
		os.unlink('potemkin.fits')
	tray.AddModule(I3TabulatorModule, 'tabbycat',
	    Photons='Photons', Source='Source', Statistics='Statistics',
	    Filename='potemkin.fits')
	
	tray.AddModule('TrashCan', 'YesWeCan')
	tray.Execute(1)
	tray.Finish()
	
if __name__ == "__main__":
	test_tray()
	 

