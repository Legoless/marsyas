/*
** Copyright (C) 1998-2010 George Tzanetakis <gtzan@cs.uvic.ca>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "Fanout.h"
#include "../common_source.h"


using std::ostringstream;
using std::vector;
using std::string;

using namespace Marsyas;

Fanout::Fanout(mrs_string name):MarSystem("Fanout", name)
{
  isComposite_ = true;
  addControls();
}

Fanout::Fanout(const Fanout& a): MarSystem(a)
{
  ctrl_enabled_ = getctrl("mrs_realvec/enabled");
  ctrl_muted_ = getctrl("mrs_realvec/muted");
}

Fanout::~Fanout()
{
}

MarSystem*
Fanout::clone() const
{
  return new Fanout(*this);
}

void
Fanout::addControls()
{
  addctrl("mrs_natural/disable", -1);
  setctrlState("mrs_natural/disable", true);
  addctrl("mrs_natural/enable", -1);
  setctrlState("mrs_natural/enable", true);

  addctrl("mrs_string/enableChild", ",");
  setctrlState("mrs_string/enableChild", true);
  addctrl("mrs_string/disableChild", ",");
  setctrlState("mrs_string/disableChild", true);

  addctrl("mrs_realvec/enabled", realvec(), ctrl_enabled_);
  addctrl("mrs_realvec/muted", realvec(), ctrl_muted_);
}

void
Fanout::myUpdate(MarControlPtr sender)
{
  MarControlAccessor acc(ctrl_enabled_);
  mrs_realvec& enabled = acc.to<mrs_realvec>();
  mrs_natural child_count = (mrs_natural) marsystems_.size();
  if (enabled.getSize() < child_count)
  {
    enabled.create(child_count);
    enabled.setval(1.0); //all children enabled by default
  }

  MarControlAccessor accMuted(ctrl_muted_);
  mrs_realvec& muted = accMuted.to<mrs_realvec>();
  if (muted.getSize() < child_count)
  {
    muted.create(child_count);
    muted.setval(0.0); //all children unmuted by default
  }

  if (child_count)
  {
    children_info_.resize((size_t)child_count);
  }

  //check child MarSystems to disable (passed as a string)
  {
    const string & child_name_to_disable = getctrl("mrs_string/disableChild")->to<mrs_string>();
    if (child_name_to_disable == "all")
    {
      for (mrs_natural i=0; i < (mrs_natural) marsystems_.size(); ++i)
      {
        enabled(i) = 0.0;
        MRSDIAG("Fanout::myUpdate(): DISABLING child: " + marsystems_[i]->getAbsPath());
      }
    }
    else
    {
      for (mrs_natural i=0; i < (mrs_natural) marsystems_.size(); ++i)
      {
        mrs_string s;
        s = marsystems_[i]->getType() + "/" + marsystems_[i]->getName();
        if (child_name_to_disable == s)
        {
          enabled(i) = 0.0;
          MRSDIAG("Fanout::myUpdate(): DISABLING child: " + marsystems_[i]->getAbsPath());
        }
      }
    }
    setctrl("mrs_string/disableChild", ",");
  }
  //check child MarSystem to disable (passed as an index)
  {
    mrs_natural child_index_to_disable = getctrl("mrs_natural/disable")->to<mrs_natural>();
    if (child_index_to_disable >= 0 && child_index_to_disable < child_count)
    {
      enabled(child_index_to_disable) = 0.0;
      MRSDIAG("Fanout::myUpdate(): DISABLING child: " + marsystems_[child_index_to_disable]->getAbsPath());
    }
    setctrl("mrs_natural/disable", -1);
  }
  //check child MarSystems to enable (passed as a string)
  {
    const string & child_name_to_enable = getctrl("mrs_string/enableChild")->to<mrs_string>();
    for (mrs_natural i=0; i < (mrs_natural) marsystems_.size(); ++i)
    {
      mrs_string s;
      s = marsystems_[i]->getType() + "/" + marsystems_[i]->getName();
      if (child_name_to_enable == s)
      {
        enabled(i) = 1.0;
        MRSDIAG("Fanout::myUpdate(): ENABLING child: " + marsystems_[i]->getAbsPath());
      }
    }
    setctrl("mrs_string/enableChild", ",");
  }
  //check child MarSystem to enable (passed as an index)
  {
    mrs_natural child_index_to_enable = getctrl("mrs_natural/enable")->to<mrs_natural>();
    if (child_index_to_enable > 0 && child_index_to_enable < child_count)
    {
      enabled(child_index_to_enable) = 1.0;
    }
    setctrl("mrs_natural/enable", -1);
  }

  for (mrs_natural i = 0; i < child_count; ++i)
  {
    children_info_[i].enabled = enabled(i) != 0.0;
  }

  if (child_count)
  {
    mrs_natural highestStabilizingDelay = ctrl_inStabilizingDelay_->to<mrs_natural>();
    //propagate in flow controls to first child
    marsystems_[0]->setctrl("mrs_natural/inObservations", inObservations_);
    marsystems_[0]->setctrl("mrs_natural/inSamples", inSamples_);
    marsystems_[0]->setctrl("mrs_real/israte", israte_);
    marsystems_[0]->setctrl("mrs_string/inObsNames", inObsNames_);
    marsystems_[0]->setctrl("mrs_natural/inStabilizingDelay", inStabilizingDelay_);
    marsystems_[0]->update();

    // update dataflow component MarSystems in order
    ostringstream oss;
    mrs_natural onObservations = 0;
    if (children_info_[0].enabled)
    {
      mrs_natural child_out_observations = marsystems_[0]->getctrl("mrs_natural/onObservations")->to<mrs_natural>();
      mrs_natural child_out_samples = marsystems_[0]->getctrl("mrs_natural/onSamples")->to<mrs_natural>();
      children_info_[0].buffer.create( child_out_observations, child_out_samples );

      onObservations += child_out_observations;
      oss << marsystems_[0]->getctrl("mrs_string/onObsNames");
      mrs_natural localStabilizingDelay = marsystems_[0]->getctrl("mrs_natural/onStabilizingDelay")->to<mrs_natural>();
      if (highestStabilizingDelay < localStabilizingDelay)
        highestStabilizingDelay = localStabilizingDelay;

    }
    for (mrs_natural i=1; i < child_count; ++i)
    {
      marsystems_[i]->setctrl("mrs_natural/inSamples", marsystems_[i-1]->getctrl("mrs_natural/inSamples"));
      marsystems_[i]->setctrl("mrs_natural/inObservations", marsystems_[i-1]->getctrl("mrs_natural/inObservations"));
      marsystems_[i]->setctrl("mrs_real/israte", marsystems_[i-1]->getctrl("mrs_real/israte"));
      marsystems_[i]->setctrl("mrs_string/inObsNames", marsystems_[0]->getctrl("mrs_string/inObsNames"));
      marsystems_[i]->setctrl("mrs_natural/inStabilizingDelay", inStabilizingDelay_);
      marsystems_[i]->update(sender);
      if (children_info_[i].enabled)
      {
        mrs_natural child_out_observations = marsystems_[i]->getctrl("mrs_natural/onObservations")->to<mrs_natural>();
        mrs_natural child_out_samples = marsystems_[i]->getctrl("mrs_natural/onSamples")->to<mrs_natural>();
        children_info_[i].buffer.create( child_out_observations, child_out_samples );

        onObservations += child_out_observations;
        oss << marsystems_[i]->getctrl("mrs_string/onObsNames");
        mrs_natural localStabilizingDelay = marsystems_[i]->getctrl("mrs_natural/onStabilizingDelay")->to<mrs_natural>();
        if (highestStabilizingDelay < localStabilizingDelay)
          highestStabilizingDelay = localStabilizingDelay;
      }
    }

    // forward flow propagation
    setctrl(ctrl_onSamples_, marsystems_[0]->getctrl("mrs_natural/onSamples")->to<mrs_natural>());
    setctrl(ctrl_onObservations_, onObservations);
    setctrl(ctrl_osrate_, marsystems_[0]->getctrl("mrs_real/osrate")->to<mrs_real>());
    setctrl(ctrl_onObsNames_, oss.str());
    setctrl(ctrl_onStabilizingDelay_, highestStabilizingDelay);

#if 0
    // update buffers between components
    mrs_natural max_observations = 0;
    mrs_natural max_samples = 0;
    for (mrs_natural i=0; i< child_count; ++i)
    {
      mrs_natural child_observation_count =
          marsystems_[i]->getctrl("mrs_natural/onObservations")->to<mrs_natural>();
      mrs_natural child_sample_count =
          marsystems_[i]->getctrl("mrs_natural/onSamples")->to<mrs_natural>();
      max_observations = std::max(max_observations, child_observation_count);
      max_samples = std::max(max_samples, child_sample_count);
    }

    buffer_.create(max_observations, max_samples);
#endif
  }
  else //if composite is empty...
    MarSystem::myUpdate(sender);
}

void
Fanout::myProcess(realvec& in, realvec& out)
{
  mrs_natural o,t;
  mrs_natural child_count = (mrs_natural) marsystems_.size();
  if (child_count)
  {
    mrs_natural out_observation_offset = 0;

    //MarControlAccessor acc(ctrl_enabled_);
    //mrs_realvec& enabled = acc.to<mrs_realvec>();

    MarControlAccessor accMuted(ctrl_muted_);
    mrs_realvec& muted = accMuted.to<mrs_realvec>();

    for (mrs_natural i = 0; i < child_count; ++i)
    {
      mrs_natural child_observation_count = children_info_[i].buffer.getRows();
      mrs_natural child_sample_count = children_info_[i].buffer.getCols();

      if (children_info_[i].enabled)//enabled child have a non-zero localIndex
      {
        //check if the child is unmuted, otherwise just use the previous output
        if(!muted(i))
        {
          marsystems_[i]->process(in, children_info_[i].buffer);

          for (o=0; o < child_observation_count; o++)
            for (t=0; t < child_sample_count; t++)
              out(out_observation_offset + o,t) = children_info_[i].buffer(o,t);
        }
        out_observation_offset += child_observation_count;
      }
    }
  }
  else //composite has no children!
  {
    MRSWARN("FanOut::process: composite has no children MarSystems - passing input to output without changes.");
    out = in;
  }
}
